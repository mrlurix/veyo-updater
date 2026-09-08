// Veyo Updater site: copy buttons, Ctrl+K search palette, sidebar filter, scrollspy, reveal.
(function () {
  "use strict";

  function copyText(t, done) {
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
  }

  // ---- copy buttons on all code blocks ----
  var pres = document.querySelectorAll("pre");
  for (var p = 0; p < pres.length; p++) {
    (function (pre) {
      if (pre.querySelector(".copy-btn")) return;
      var btn = document.createElement("button");
      btn.className = "copy-btn";
      btn.textContent = "Copy";
      btn.addEventListener("click", function () {
        var clone = pre.cloneNode(true);
        var b = clone.querySelector(".copy-btn");
        if (b) b.remove();
        var t = clone.textContent;
        copyText(t, function () {
          btn.textContent = "Copied";
          setTimeout(function () { btn.textContent = "Copy"; }, 1200);
        });
      });
      pre.appendChild(btn);
    })(pres[p]);
  }

  // ---- search index ----
  var INDEX = [
    { page: "Home", t: "Download Veyo Updater", s: "Portable zip, latest release", u: "https://github.com/mrlurix/veyo-updater/releases/latest", k: "download install setup exe zip release" },
    { page: "Home", t: "Features overview", s: "What the app does", u: "index.html#everything-you-need-to-update", k: "features portable elevated animations" },
    { page: "Home", t: "How it works", s: "Three steps", u: "index.html#up-and-running-in-a-minute", k: "how steps guide" },
    { page: "Home", t: "Screenshot", s: "App preview", u: "index.html#take-a-look", k: "screenshot preview look ui" },
    { page: "Docs", t: "Getting started", s: "Install and run", u: "docs.html#start", k: "start install run requirements windows winget" },
    { page: "Docs", t: "Updating apps", s: "Check, single, selected, all", u: "docs.html#updating", k: "update upgrade check selected all button uac" },
    { page: "Docs", t: "Live progress", s: "Stage, percent, counter", u: "docs.html#progress", k: "progress percent bar stage downloading installing" },
    { page: "Docs", t: "Settings reference", s: "Timers, countdown, finish action", u: "docs.html#settings", k: "settings timer countdown auto check interval finish shutdown close ini" },
    { page: "Docs", t: "Automation examples", s: "Overnight updates", u: "docs.html#automation", k: "automation overnight schedule" },
    { page: "Docs", t: "Building from source", s: "MSVC, CMake, MinGW", u: "docs.html#build", k: "build source compile cmake visual studio" },
    { page: "Docs", t: "Troubleshooting", s: "Common problems", u: "docs.html#trouble", k: "troubleshoot error problem fix winget missing" },
    { page: "Docs", t: "Changelog", s: "Version history", u: "docs.html#changelog", k: "changelog version history release notes" },
    { page: "Docs", t: "License", s: "MIT", u: "docs.html#license", k: "license mit open source" },
    { page: "GitHub", t: "Repository", s: "Source code and issues", u: "https://github.com/mrlurix/veyo-updater", k: "github source code repo issues" }
  ];

  function esc(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  // ---- command palette (built once, used on every page) ----
  var overlay = null, palInput = null, palResults = null, selIdx = -1, curHits = [];
  function buildPalette() {
    overlay = document.createElement("div");
    overlay.className = "sp-overlay";
    overlay.setAttribute("hidden", "");
    overlay.innerHTML =
      '<div class="sp-modal" role="dialog" aria-label="Search">' +
      '<div class="sp-input-row">' +
      '<svg viewBox="0 0 24 24"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>' +
      '<input type="text" placeholder="Search docs..." aria-label="Search docs">' +
      "</div>" +
      '<div class="sp-results"></div>' +
      '<div class="sp-foot"><span><kbd>↑↓</kbd>navigate</span><span><kbd>⏎</kbd>open</span><span><kbd>esc</kbd>close</span></div>' +
      "</div>";
    document.body.appendChild(overlay);
    palInput = overlay.querySelector("input");
    palResults = overlay.querySelector(".sp-results");
    overlay.addEventListener("mousedown", function (ev) { if (ev.target === overlay) closePalette(); });
    palInput.addEventListener("input", function () { renderPal(palInput.value); });
    palInput.addEventListener("keydown", function (ev) {
      if (ev.key === "ArrowDown") { ev.preventDefault(); moveSel(1); }
      else if (ev.key === "ArrowUp") { ev.preventDefault(); moveSel(-1); }
      else if (ev.key === "Enter") {
        var link = palResults.querySelector(".sp-item.sel") || palResults.querySelector(".sp-item");
        if (link) window.location.href = link.getAttribute("href");
      } else if (ev.key === "Escape") { closePalette(); }
    });
  }
  function hl(text, q) {
    var i = text.toLowerCase().indexOf(q);
    if (i < 0 || !q) return esc(text);
    return esc(text.slice(0, i)) + "<mark>" + esc(text.slice(i, i + q.length)) + "</mark>" + esc(text.slice(i + q.length));
  }
  function renderPal(qRaw) {
    var q = (qRaw || "").trim().toLowerCase();
    curHits = INDEX.filter(function (e) {
      if (!q) return true;
      return (e.t + " " + e.s + " " + e.k + " " + e.page).toLowerCase().indexOf(q) !== -1;
    }).slice(0, 8);
    selIdx = curHits.length ? 0 : -1;
    if (!curHits.length) {
      palResults.innerHTML = '<div class="sp-empty">No results</div>';
      return;
    }
    palResults.innerHTML = curHits.map(function (e, i) {
      return '<a class="sp-item' + (i === 0 ? " sel" : "") + '" href="' + e.u + '">' +
        '<div class="sp-page">' + esc(e.page) + "</div>" +
        '<div class="sp-sec">' + hl(e.t, q) + "</div>" +
        '<div class="sp-snip">' + esc(e.s) + "</div></a>";
    }).join("");
  }
  function moveSel(d) {
    if (!curHits.length) return;
    selIdx = (selIdx + d + curHits.length) % curHits.length;
    var items = palResults.querySelectorAll(".sp-item");
    for (var i = 0; i < items.length; i++) {
      if (i === selIdx) items[i].classList.add("sel");
      else items[i].classList.remove("sel");
    }
    if (items[selIdx] && items[selIdx].scrollIntoView) items[selIdx].scrollIntoView({ block: "nearest" });
  }
  function openPalette() {
    if (!overlay) buildPalette();
    overlay.removeAttribute("hidden");
    requestAnimationFrame(function () { overlay.classList.add("show"); });
    palInput.value = "";
    renderPal("");
    setTimeout(function () { palInput.focus(); }, 30);
  }
  function closePalette() {
    if (!overlay) return;
    overlay.classList.remove("show");
    setTimeout(function () { if (overlay) overlay.setAttribute("hidden", ""); }, 180);
  }
  var triggers = document.querySelectorAll("#sp-open");
  for (var ti = 0; ti < triggers.length; ti++) {
    triggers[ti].addEventListener("click", openPalette);
  }
  document.addEventListener("keydown", function (ev) {
    if ((ev.ctrlKey || ev.metaKey) && (ev.key === "k" || ev.key === "K")) {
      ev.preventDefault();
      if (overlay && !overlay.hasAttribute("hidden")) closePalette();
      else openPalette();
    }
  });

  // ---- docs sidebar: filter ----
  var filter = document.getElementById("side-filter");
  if (filter) {
    filter.addEventListener("input", function () {
      var q = filter.value.trim().toLowerCase();
      var links = document.querySelectorAll(".sidebar a");
      for (var i = 0; i < links.length; i++) {
        var show = !q || links[i].textContent.toLowerCase().indexOf(q) !== -1;
        links[i].style.display = show ? "" : "none";
      }
      var groups = document.querySelectorAll(".sidebar .nav-group");
      for (var g = 0; g < groups.length; g++) {
        var el = groups[g].nextElementSibling;
        var any = false;
        while (el && !el.classList.contains("nav-group")) {
          if (el.tagName === "A" && el.style.display !== "none") { any = true; break; }
          el = el.nextElementSibling;
        }
        groups[g].style.display = (!q || any) ? "" : "none";
      }
    });
  }

  // ---- docs scrollspy (band pinned near top + instant feedback on click) ----
  var links = Array.prototype.slice.call(document.querySelectorAll(".sidebar a[href^=\"#\"]"));
  var spyLocked = false, spyTimer = null;
  function spyActivate(a) {
    links.forEach(function (x) { x.classList.remove("active"); });
    if (a) a.classList.add("active");
  }
  links.forEach(function (a) {
    a.addEventListener("click", function () {
      spyActivate(a);
      spyLocked = true;
      if (spyTimer) clearTimeout(spyTimer);
      spyTimer = setTimeout(function () { spyLocked = false; }, 900);
    });
  });
  if (links.length && "IntersectionObserver" in window) {
    var map = {};
    links.forEach(function (a) { map[a.getAttribute("href").slice(1)] = a; });
    var obs = new IntersectionObserver(function (entries) {
      if (spyLocked) return;
      entries.forEach(function (e) {
        if (e.isIntersecting) spyActivate(map[e.target.id]);
      });
    }, { rootMargin: "-96px 0px -75% 0px" });
    Object.keys(map).forEach(function (id) {
      var el = document.getElementById(id);
      if (el) obs.observe(el);
    });
  }

  // ---- mobile sidebar ----
  var tog = document.getElementById("nav-toggle");
  var side = document.getElementById("sidebar");
  if (tog && side) {
    tog.addEventListener("click", function () { side.classList.toggle("open"); });
    side.addEventListener("click", function (ev) {
      if (ev.target.tagName === "A" && window.innerWidth <= 860) side.classList.remove("open");
    });
  }

  // ---- scroll reveal ----
  var revs = document.querySelectorAll(".reveal");
  if (revs.length && "IntersectionObserver" in window) {
    var robs = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) { e.target.classList.add("in"); robs.unobserve(e.target); }
      });
    }, { threshold: 0.12 });
    for (var r = 0; r < revs.length; r++) robs.observe(revs[r]);
  } else {
    for (var r2 = 0; r2 < revs.length; r2++) revs[r2].classList.add("in");
  }
})();
