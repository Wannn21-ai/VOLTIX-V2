const pages = [
  { id: "dashboard", href: "index.html", icon: "⌁", label: "Dashboard" },
  { id: "history", href: "history.html", icon: "◫", label: "Histori" },
  { id: "advanced", href: "advanced.html", icon: "⌘", label: "Advanced" },
  { id: "settings", href: "settings.html", icon: "⚙", label: "Pengaturan" }
];

export function renderShell(activePage, title) {
  const sidebar = document.getElementById("sidebar");
  const topbar = document.getElementById("topbar");
  if (!sidebar || !topbar) return;

 
  sidebar.innerHTML = `
    <div class="sidebar-logo">
      <img class="sidebar-logo-icon" src="assets/logo/LOGO.png" alt="Logo VOLTIX">
      <div><div class="sidebar-logo-text">VOLTIX</div><div class="sidebar-logo-sub">ENERGY MONITOR V2</div></div>
    </div>
    <div class="sidebar-section-label">Monitoring</div>
    <nav class="sidebar-nav" aria-label="Navigasi utama">
      ${pages.map(page => `<a href="${page.href}" class="${page.id === activePage ? "active" : ""}"><span class="nav-icon">${page.icon}</span>${page.label}</a>`).join("")}
    </nav>
    <div class="sidebar-bottom">
      <div class="sidebar-device"><strong>VOLTIX-001</strong><span>Satu alat · satu beban aktif</span></div>
    </div>`;

  topbar.innerHTML = `
    <button class="topbar-menu-btn" id="menu-toggle" type="button" aria-label="Buka menu">☰</button>
    <div class="topbar-title">${title}</div>
    <div class="topbar-status"><span class="status-dot transition" id="global-status-dot"></span><span id="global-status-text">Menunggu alat</span></div>`;

const mobileQuery = window.matchMedia(
  "(max-width: 768px)"
);

const menuButton =
  document.getElementById("menu-toggle");

const backdrop = document.createElement("div");

backdrop.className = "sidebar-backdrop";
document.body.appendChild(backdrop);

function setSidebarOpen(open) {
  sidebar.classList.toggle("open", open);

  backdrop.classList.toggle(
    "show",
    open && mobileQuery.matches
  );

  menuButton?.setAttribute(
    "aria-expanded",
    String(open)
  );

  menuButton?.setAttribute(
    "aria-label",
    open ? "Tutup menu" : "Buka menu"
  );
}

setSidebarOpen(!mobileQuery.matches);

menuButton?.addEventListener("click", () => {
  setSidebarOpen(
    !sidebar.classList.contains("open")
  );
});

backdrop.addEventListener("click", () => {
  setSidebarOpen(false);
});

sidebar.querySelectorAll("a").forEach(link => {
  link.addEventListener("click", () => {
    if (mobileQuery.matches) {
      setSidebarOpen(false);
    }
  });
});

mobileQuery.addEventListener("change", event => {
  setSidebarOpen(!event.matches);
});
}

export function setGlobalStatus(online, label) {
  const dot = document.getElementById("global-status-dot");
  const text = document.getElementById("global-status-text");
  if (dot) dot.className = `status-dot ${online ? "online" : "offline"}`;
  if (text) text.textContent = label || (online ? "Alat online" : "Alat offline");
}

export function showToast(message, type = "") {
  let stack = document.querySelector(".toast-stack");
  if (!stack) {
    stack = document.createElement("div");
    stack.className = "toast-stack";
    document.body.appendChild(stack);
  }
  const toast = document.createElement("div");
  toast.className = `toast ${type}`;
  toast.textContent = message;
  stack.appendChild(toast);
  window.setTimeout(() => toast.remove(), 4200);
}

export function formatDuration(seconds = 0) {
  const total = Math.max(0, Math.floor(Number(seconds) || 0));
  const h = String(Math.floor(total / 3600)).padStart(2, "0");
  const m = String(Math.floor((total % 3600) / 60)).padStart(2, "0");
  const s = String(total % 60).padStart(2, "0");
  return `${h}:${m}:${s}`;
}

export function formatRupiah(value = 0) {
  return new Intl.NumberFormat("id-ID", { style: "currency", currency: "IDR", maximumFractionDigits: 0 }).format(Number(value) || 0);
}

