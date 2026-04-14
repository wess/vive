# Homebrew formula for Vive.
#
# To publish:
#   1. Create a tap repo: github.com/wess/homebrew-tap
#   2. Copy this file to Formula/vive.rb in that repo
#   3. Update `version` and the four `sha256` values from the release's .sha256 files
#   4. Users install via: brew install wess/tap/vive
#
# The release.yml workflow prints checksums; bump them here on each release
# (or wire up an auto-bump action such as dawidd6/action-homebrew-bump-formula).

class Vive < Formula
  desc "Smart MCP harness for AI agent orchestration"
  homepage "https://github.com/wess/vive"
  version "0.1.0"
  license "MIT"

  depends_on "cjson"
  depends_on "ncurses"
  depends_on "sqlite"

  on_macos do
    on_arm do
      url "https://github.com/wess/vive/releases/download/v#{version}/vive-#{version}-darwin-arm64.tar.gz"
      sha256 "REPLACE_WITH_DARWIN_ARM64_SHA"
    end
    on_intel do
      url "https://github.com/wess/vive/releases/download/v#{version}/vive-#{version}-darwin-x86_64.tar.gz"
      sha256 "REPLACE_WITH_DARWIN_X86_64_SHA"
    end
  end

  on_linux do
    on_arm do
      url "https://github.com/wess/vive/releases/download/v#{version}/vive-#{version}-linux-arm64.tar.gz"
      sha256 "REPLACE_WITH_LINUX_ARM64_SHA"
    end
    on_intel do
      url "https://github.com/wess/vive/releases/download/v#{version}/vive-#{version}-linux-x86_64.tar.gz"
      sha256 "REPLACE_WITH_LINUX_X86_64_SHA"
    end
  end

  def install
    bin.install "vive"
    doc.install "README.md" if File.exist?("README.md")
  end

  test do
    assert_match "vive", shell_output("#{bin}/vive --help 2>&1", 0..255)
  end
end
