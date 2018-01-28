using System;
using System.IO;
using Microsoft.Win32.SafeHandles;

namespace LxRunOffline {
	static class FileSystem {
		const int DeletionRetryCount = 3;

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
			string toNtPath(string path) => $@"\??\{Path.GetFullPath(path)}";

			using (var hDir = PInvoke.GetFileHandle(toNtPath(oldPath), true, false, false)) {
				CheckFileHandle(hDir, oldPath);

				while (true) {
					if (!PInvoke.EnumerateDirectory(hDir, out var fileName, out var isDir)) {
						Utils.Error($"Couldn't get the contents of the directory \"{oldPath}\".");
					}
					if (fileName == null) break;
					if (fileName == "." || fileName == "..") continue;

					var oldFilePath = Path.Combine(oldPath, fileName);
					var newFilePath = Path.Combine(newPath, fileName);

					using (var hOld = PInvoke.GetFileHandle(toNtPath(oldFilePath), isDir, false, false))
					using (var hNew = PInvoke.GetFileHandle(toNtPath(newFilePath), isDir, true, true)) {
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

		public static void MoveDirectory(string oldPath, string newPath) {
			CopyDirectory(oldPath, newPath);
			DeleteDirectory(oldPath);
		}
	}
}
