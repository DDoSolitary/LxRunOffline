using System;
using System.Net;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;

namespace LxRunOfflineRedirect {
	[Route("[controller]/[action]/{version?}")]
	public class DownloadController : Controller {
		async Task<string> DownloadStringAsync(string url) {
			try {
				using (var client = new WebClient()) {
					return await client.DownloadStringTaskAsync(url);
				}
			} catch (WebException e) {
				if (e.Status == WebExceptionStatus.ProtocolError &&
					((HttpWebResponse)e.Response).StatusCode == HttpStatusCode.NotFound) {
					return null;
				}
				throw;
			}
		}

		public IActionResult UbuntuFromMS(int version) {
			switch (version) {
			case 14:
				return Redirect("https://wsldownload.azureedge.net/14.04.5.3-server-cloudimg-amd64-root.tar.gz");
			case 16:
				return Redirect("https://wsldownload.azureedge.net/16.04.2-server-cloudimg-amd64-root.tar.gz");
			default:
				return NotFound();
			}
		}

		public IActionResult Ubuntu(string version)
			=> Redirect($"https://github.com/tianon/docker-brew-ubuntu-core/raw/dist-amd64/{version}/ubuntu-{version}-core-cloudimg-amd64-root.tar.gz");

		public async Task<IActionResult> ArchLinux() {
			var url = "https://mirrors.kernel.org/archlinux/iso/latest/";
			var fileList = await DownloadStringAsync(url + "sha1sums.txt");
			if (fileList == null) return NotFound();
			return Redirect(url + fileList.Split('\n')[1].Split(' ', StringSplitOptions.RemoveEmptyEntries)[1]);
		}

		public async Task<IActionResult> Alpine(string version = "edge") {
			var url = $"http://dl-cdn.alpinelinux.org/alpine/{version}/releases/x86_64/";
			var releaseList = await DownloadStringAsync(url + "latest-releases.yaml");
			if (releaseList == null) return NotFound();
			return Redirect(url + Regex.Match(releaseList, @"\balpine-minirootfs-.*\.tar\.gz\b").Value);
		}

		public async Task<IActionResult> Fedora(string version = "rawhide") {
			var url = $"https://github.com/fedora-cloud/docker-brew-fedora/raw/{version}/x86_64/";
			var dockerFile = await DownloadStringAsync(url + "Dockerfile");
			if (dockerFile == null) return NotFound();
			return Redirect(url + Regex.Match(dockerFile, @"\bfedora-.*\.tar\.xz\b").Value);
		}

		public IActionResult openSUSE(string version = "Tumbleweed") {
			return Redirect($"https://github.com/openSUSE/docker-containers-build/raw/openSUSE-{version}/x86_64/openSUSE-{version}.base.x86_64.tar.xz");
		}

		[Route("/")]
		public string Home() => "This is the download redirection site for the LxRunOffline project.";
	}
}
