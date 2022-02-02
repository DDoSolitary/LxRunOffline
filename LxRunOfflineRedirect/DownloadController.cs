using System;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Newtonsoft.Json.Linq;

namespace LxRunOfflineRedirect {
	[Route("[controller]/[action]/{version?}")]
	public class DownloadController : Controller {
		async Task<string> DownloadStringAsync(string url) {
			try {
				using var client = new WebClient();
				client.Headers.Add("User-Agent", nameof(LxRunOfflineRedirect));
				return await client.DownloadStringTaskAsync(url);
			} catch (WebException e) {
				if (e.Status == WebExceptionStatus.ProtocolError &&
					((HttpWebResponse)e.Response).StatusCode == HttpStatusCode.NotFound) {
					return null;
				}
				throw;
			}
		}

		public IActionResult UbuntuFromMS(int version) {
			return version switch {
				14 => Redirect("https://wsldownload.azureedge.net/14.04.5.3-server-cloudimg-amd64-root.tar.gz"),
				16 => Redirect("https://wsldownload.azureedge.net/16.04.2-server-cloudimg-amd64-root.tar.gz"),
				_ => NotFound(),
			};
		}

		public IActionResult Ubuntu(string version) {
			version = version.ToLower();
			string filename;
			if (version == "trusty" || version == "xenial") {
				filename = $"ubuntu-{version}-core-cloudimg-amd64-root.tar.gz"
			} else {
				filename = $"ubuntu-{version}-oci-amd64-root.tar.gz"
			}
			return Redirect($"https://github.com/tianon/docker-brew-ubuntu-core/raw/dist-amd64/{version}/{filename}");
		}

		public async Task<IActionResult> ArchLinux() {
			var url = "https://mirrors.kernel.org/archlinux/iso/latest/";
			var fileList = await DownloadStringAsync(url + "sha1sums.txt");
			if (fileList == null) return NotFound();
			return Redirect(url + fileList.Split('\n')[1].Split(' ', StringSplitOptions.RemoveEmptyEntries)[1]);
		}

		public async Task<IActionResult> Alpine(string version = "edge") {
			var url = $"http://dl-cdn.alpinelinux.org/alpine/{version.ToLower()}/releases/x86_64/";
			var releaseList = await DownloadStringAsync(url + "latest-releases.yaml");
			if (releaseList == null) return NotFound();
			return Redirect(url + Regex.Match(releaseList, @"\balpine-minirootfs-.*\.tar\.gz\b").Value);
		}

		public async Task<IActionResult> Fedora(string version = "rawhide") {
			var repoSlug = "fedora-cloud/docker-brew-fedora";
			if (version.ToLowerInvariant() == "rawhide") {
				var branches = JArray.Parse(await DownloadStringAsync($"https://api.github.com/repos/{repoSlug}/branches"));
				version = branches
					.Select(b => (string)((JObject)b)["name"])
					.Where(name => int.TryParse(name, out _))
					.OrderByDescending(int.Parse)
					.First().ToString();
			}
			var url = $"https://github.com/{repoSlug}/raw/{version.ToLower()}/x86_64/";
			var dockerFile = await DownloadStringAsync(url + "Dockerfile");
			if (dockerFile == null) return NotFound();
			return Redirect(url + Regex.Match(dockerFile, @"\bfedora-.*\.tar\.xz\b").Value);
		}

		public IActionResult openSUSE(string version = "Tumbleweed") {
			version = char.ToUpper(version[0]) + version.Substring(1).ToLower();
			return Redirect($"https://github.com/openSUSE/docker-containers-build/raw/openSUSE-{version}/x86_64/openSUSE-{version}.base.x86_64.tar.xz");
		}

		public IActionResult Debian(string version = "sid")
			=> Redirect($"https://github.com/debuerreotype/docker-debian-artifacts/raw/dist-amd64/{version.ToLower()}/rootfs.tar.xz");

		[Route("/")]
		public string Home() => "This is the download redirection site for the LxRunOffline project.";
	}
}
