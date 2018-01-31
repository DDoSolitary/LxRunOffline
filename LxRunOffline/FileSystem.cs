using System;
using System.IO;
using Microsoft.Win32.SafeHandles;
using ICSharpCode.SharpZipLib.Tar;
using System.Text;
using System.Runtime.InteropServices;

namespace LxRunOffline {
	static class FileSystem {
		[StructLayout(LayoutKind.Sequential)]
		public class LxssEaData {
			public short Reserved1;
			public short Version = 1;
			public int Mode;
			public int Uid;
			public int Gid;
			public int Reserved2;
			public int AtimeNsec;
			public int MtimeNsec;
			public int CtimeNsec;
			public long Atime;
			public long Mtime;
			public long Ctime;
		}

		const int DeletionRetryCount = 3;

		[DllImport("LxssFileSystem.dll", CallingConvention = CallingConvention.Cdecl)]
		static extern bool EnumerateDirectory(
			SafeFileHandle hFile,
			[MarshalAs(UnmanagedType.LPWStr)]out string fileName,
			out bool directory
		);

		[DllImport("LxssFileSystem.dll", CallingConvention = CallingConvention.Cdecl)]
		static extern SafeFileHandle GetFileHandle(
			[MarshalAs(UnmanagedType.LPWStr)]string ntPath,
			bool directory,
			bool create,
			bool write
		);

		[DllImport("LxssFileSystem.dll", CallingConvention = CallingConvention.Cdecl)]
		static extern bool CopyLxssEa(SafeFileHandle hFrom, SafeFileHandle hTo);

		[DllImport("LxssFileSystem.dll", CallingConvention = CallingConvention.Cdecl)]
		static extern bool SetLxssEa(SafeFileHandle hFile, LxssEaData data, int dataLength);

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
			using (var hDir = GetFileHandle(oldPath.ToNtPath(), true, false, false)) {
				CheckFileHandle(hDir, oldPath);

				while (true) {
					if (!EnumerateDirectory(hDir, out var fileName, out var isDir)) {
						Utils.Error($"Couldn't get the contents of the directory \"{oldPath}\".");
					}
					if (fileName == null) break;
					if (fileName == "." || fileName == "..") continue;

					var oldFilePath = Path.Combine(oldPath, fileName);
					var newFilePath = Path.Combine(newPath, fileName);

					using (var hOld = GetFileHandle(oldFilePath.ToNtPath(), isDir, false, false))
					using (var hNew = GetFileHandle(newFilePath.ToNtPath(), isDir, true, true)) {
						CheckFileHandle(hOld, oldFilePath);
						CheckFileHandle(hNew, newFilePath);

						if (!CopyLxssEa(hOld, hNew)) {
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
						using (var hNew = GetFileHandle(newFilePath.ToNtPath(), type == TarHeader.LF_DIR, true, true)) {
							CheckFileHandle(hNew, newFilePath);

							var eaData = new LxssEaData {
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

							SetLxssEa(hNew, eaData, Marshal.SizeOf(typeof(LxssEaData)));

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
