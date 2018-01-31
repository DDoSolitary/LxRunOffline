using System;
using System.IO;
using Microsoft.Win32.SafeHandles;
using ICSharpCode.SharpZipLib.Tar;
using System.Text;
using System.Runtime.InteropServices;

namespace LxRunOffline {
	static class FileSystem {
		const int DeletionRetryCount = 3;

		static string ToNtPath(this string path) => $@"\??\{Path.GetFullPath(path)}";

		static string ToWslPath(this string path) {
			var sb = new StringBuilder();
			foreach (var c in path.TrimStart('/')) {
				if (c == '/') sb.Append('\\');
				else if (c >= 1 && c <= 31 || c == '<' || c == '>' || c == ':' || c == '"' || c == '\\' || c == '|' || c == '*' || c == '#') {
					sb.Append('#');
					sb.Append(((int)c).ToString("X4"));
				}
			}
			return sb.ToString();
		}

		static void CheckFileHandle(SafeFileHandle hFile, string path) {
			if (hFile.IsInvalid) {
				Utils.Error($"Couldn't open the file or directory \"{path}\".");
			}
		}

		public static void DeleteDirectory(string path) {
			Utils.Log($"Deleting the directory \"{path}\".");
			var retryCount = DeletionRetryCount;
			while (true) {
				retryCount--;
				try {
					Directory.Delete(path, true);
					return;
				} catch (Exception e) {
					Utils.Warning($"Couldn't delete the directory \"{path}\": {e.Message}");
					if (retryCount == 0) {
						Utils.Warning($"You may have to delete it manually.");
					} else {
						Utils.Warning($"Retrying.");
					}
				}
			}
		}

		public static void CopyDirectory(string oldPath, string newPath) {
			using (var hDir = PInvoke.GetFileHandle(oldPath.ToNtPath(), true, false, false)) {
				CheckFileHandle(hDir, oldPath);

				while (true) {
					if (!PInvoke.EnumerateDirectory(hDir, out var fileName, out var isDir)) {
						Utils.Error($"Couldn't get the contents of the directory \"{oldPath}\".");
					}
					if (fileName == null) break;
					if (fileName == "." || fileName == "..") continue;

					var oldFilePath = Path.Combine(oldPath, fileName);
					var newFilePath = Path.Combine(newPath, fileName);

					using (var hOld = PInvoke.GetFileHandle(oldFilePath.ToNtPath(), isDir, false, false))
					using (var hNew = PInvoke.GetFileHandle(newFilePath.ToNtPath(), isDir, true, true)) {
						CheckFileHandle(hOld, oldFilePath);
						CheckFileHandle(hNew, newFilePath);

						if (!PInvoke.CopyLxssEa(hOld, hNew)) {
							Utils.Error($"Couldn't copy extended attributes from \"{oldFilePath}\" to \"{newFilePath}\".");
						}

						if (!isDir) {
							using (var fsOld = new FileStream(hOld, FileAccess.Read))
							using (var fsNew = new FileStream(hNew, FileAccess.ReadWrite)) {
								fsOld.CopyTo(fsNew);
							}
						}
					}

					if (isDir) CopyDirectory(oldFilePath, newFilePath);
				}
			}
		}

		public static void ExtractTar(Stream stream, string targetPath) {
			using (var tar = new TarInputStream(stream)) {
				while (true) {
					var entry = tar.GetNextEntry();
					if (entry == null) break;

					var type = entry.TarHeader.TypeFlag;
					var newFilePath = Path.Combine(targetPath, entry.Name.ToWslPath());

					if (type == TarHeader.LF_LINK) {
						// TODO: Create hard links.
					} else {
						using (var hNew = PInvoke.GetFileHandle(newFilePath.ToNtPath(), type == TarHeader.LF_DIR, true, true)) {
							CheckFileHandle(hNew, newFilePath);

							var eaData = new PInvoke.LxssEaData {
								Mode = entry.TarHeader.Mode,
								Uid = entry.UserId,
								Gid = entry.GroupId
							};
							DateTimeOffset modTime = DateTime.SpecifyKind(entry.ModTime, DateTimeKind.Utc);
							eaData.Atime = eaData.Mtime = eaData.Ctime = modTime.ToUnixTimeSeconds();


							switch (type) {
							case TarHeader.LF_DIR:
								eaData.Mode |= 0x4000;
								break;
							case TarHeader.LF_SYMLINK:
								eaData.Mode |= 0xa000;
								break;
							case TarHeader.LF_OLDNORM:
								goto case TarHeader.LF_NORMAL;
							case TarHeader.LF_NORMAL:
								eaData.Mode |= 0x8000;
								break;
							default:
								Utils.Warning($"Ignoring unsupported file type {(char)type}.");
								continue;
							}

							PInvoke.SetLxssEa(hNew, eaData, Marshal.SizeOf(typeof(PInvoke.LxssEaData)));

							if (type == TarHeader.LF_DIR) continue;
							using (var fsNew = new FileStream(hNew, FileAccess.ReadWrite)) {
								if (entry.TarHeader.TypeFlag == TarHeader.LF_SYMLINK) {
									var linkData = Encoding.UTF8.GetBytes(entry.TarHeader.LinkName);
									fsNew.Write(linkData, 0, linkData.Length);
								} else {
									tar.CopyEntryContents(fsNew);
								}
							}
						}
					}
				}
			}
		}
	}
}
