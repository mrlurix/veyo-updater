// Veyo Updater site: copy button, docs search (Ctrl+K), scrollspy, mobile menu.
(function () {
  "use strict";

  // ---- copy install command ----
  var copyBtn = document.getElementById("copy-btn");
  if (copyBtn) {
    copyBtn.addEventListener("click", function () {
      var t = document.getElementById("install-cmd").textContent;
      function done() {
        copyBtn.textContent = "Copied";
        setTimeout(function () { copyBtn.textContent = "Copy"; }, 1200);
      }
      function legacy() {
        var ta = document.createElement("textarea");
        ta.value = t;
        document.body.appendChild(ta);
        ta.select();
        try { document.execCommand("copy"); done(); } catch (e) { /* noop */ }
        document.body.removeChild(ta);
      }
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(t).then(done, legacy);
      } else { legacy(); }
    });
  }

  // ---- search index ----
  var INDEX = [
    { t: "Download Veyo Updater", d: "Portable zip • latest release", u: "https://github.com/mrlurix/veyo-updater/releases/latest", k: "download install setup exe zip release" },
    { t: "Getting started", d: "Docs • install & run", u: "docs.html#start", k: "start install run requirements windows winget" },
    { t: "Updating apps", d: "Docs • check, single, selected, all", u: "docs.html#updating", k: "update upgrade check selected all button uac" },
    { t: "Live progress", d: "Docs • stage, percent, counter", u: "docs.html#progress", k: "progress percent bar stage downloading installing" },
    { t: "Settings reference", d: "Docs • timers, countdown, finish action", u: "docs.html#settings", k: "settings timer countdown auto check interval finish shutdown close ini" },
    { t: "Automation examples", d: "Docs • overnight updates", u: "docs.html#automation", k: "automation overnight schedule" },
    { t: "Building from source", d: "Docs • MSVC, CMake, MinGW", u: "docs.html#build", k: "build source compile cmake visual studio" },
    { t: "Troubleshooting", d: "Docs • common problems", u: "docs.html#trouble", k: "troubleshoot error problem fix winget missing" },
    { t: "Changelog", d: "Docs • version history", u: "docs.html#changelog", k: "changelog version history release notes" },
    { t: "License", d: "Docs • MIT", u: "docs.html#license", k: "license mit open source" },
    { t: "Features overview", d: "Home • what the app does", u: "index.html#features", k: "features portable elevated animations" },
    { t: "How it works", d: "Home • three steps", u: "index.html#how", k: "how steps guide" },
    { t: "Screenshot", d: "Home • app preview", u: "index.html#shot", k: "screenshot preview look ui" },
    { t: "GitHub repository", d: "Source code & issues", u: "https://github.com/mrlurix/veyo-updater", k: "github source code repo issues" }
  ];

  function wireSearch(box) {
    var input = box.querySelector("input");
    var panel = box.querySelector("#search-results") || box.querySelector(".sr-panel");
    if (!input || !panel) return;
    function close() { panel.classList.remove("open"); panel.innerHTML = ""; }
    input.addEventListener("input", function () {
      var q = input.value.trim().toLowerCase();
      if (q.length < 2) { close(); return; }
      var hits = INDEX.filter(function (e) {
        return (e.t + " " + e.d + " " + e.k).toLowerCase().indexOf(q) !== -1;
      }).slice(0, 7);
      if (!hits.length) { panel.innerHTML = '<div class="sr-empty">No results</div>'; panel.classList.add("open"); return; }
      panel.innerHTML = hits.map(function (e, i) {
        return '<a class="sr-item" data-i="' + i + '" href="' + e.u + '"><b>' + e.t + '</b><span>' + e.d + '</span></a>';
      }).join("");
      panel.classList.add("open");
      var links = panel.querySelectorAll(".sr-item");
      for (var j = 0; j < links.length; j++) {
        (function (el, entry) {
          el.addEventListener("mousedown", function (ev) { ev.preventDefault(); window.location.href = entry.u; });
        })(links[j], hits[j]);
      }
    });
    input.addEventListener("keydown", function (ev) {
      if (ev.key === "Enter") {
        var first = panel.querySelector(".sr-item");
        if (first) window.location.href = first.getAttribute("href");
      } else if (ev.key === "Escape") { close(); input.blur(); }
    });
    input.addEventListener("blur", function () { setTimeout(close, 150); });
  }
  var boxes = document.querySelectorAll(".searchbox");
  for (var i = 0; i < boxes.length; i++) wireSearch(boxes[i]);

  document.addEventListener("keydown", function (ev) {
    if ((ev.ctrlKey || ev.metaKey) && (ev.key === "k" || ev.key === "K")) {
      var inp = document.querySelector(".searchbox input");
      if (inp) { ev.preventDefault(); inp.focus(); }
    }
  });

  // ---- mobile menu ----
  var tog = document.getElementById("nav-toggle");
  var menu = document.getElementById("mobile-menu");
  if (tog && menu) {
    tog.addEventListener("click", function () { menu.classList.toggle("open"); });
  }

  // ---- docs scrollspy ----
  var links = Array.prototype.slice.call(document.querySelectorAll(".slink[href^=\"#\"]"));
  if (links.length && "IntersectionObserver" in window) {
    var map = {};
    links.forEach(function (a) { map[a.getAttribute("href").slice(1)] = a; });
    var obs = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) {
          links.forEach(function (a) { a.classList.remove("active"); });
          var a = map[e.target.id];
          if (a) a.classList.add("active");
        }
      });
    }, { rootMargin: "-30% 0px -60% 0px" });
    Object.keys(map).forEach(function (id) {
      var el = document.getElementById(id);
      if (el) obs.observe(el);
    });
  }
})();
