const API = "cgi-bin/lostfound.cgi";
const grid = document.getElementById("items-grid");
const search = document.getElementById("search");
const note = document.getElementById("status-note");
let reports = [];
let activeFilter = "All";

function escapeHTML(value="") {
  return String(value).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
}
function render() {
  const term = search.value.trim().toLowerCase();
  const shown = reports.filter(r => (activeFilter === "All" || r.type === activeFilter) &&
    [r.item,r.category,r.location,r.description].join(" ").toLowerCase().includes(term));
  if (!shown.length) { grid.innerHTML = '<div class="empty">No matching reports yet. Try another search or add a report.</div>'; return; }
  grid.innerHTML = shown.map(r => `<article class="item-card">
    <div class="card-top"><span class="pill ${r.type.toLowerCase()}">${escapeHTML(r.type)}</span><span class="category">${escapeHTML(r.category)}</span></div>
    <h3>${escapeHTML(r.item)}</h3><p class="item-desc">${escapeHTML(r.description || "No extra description provided.")}</p>
    <div class="meta"><span>⌖ &nbsp;<b>${escapeHTML(r.location)}</b></span><span>▦ &nbsp;${escapeHTML(r.date)}</span><span>Reported by <b>${escapeHTML(r.reporter)}</b></span><span>Contact: <b>${escapeHTML(r.contact)}</b></span></div>
  </article>`).join("");
}
async function loadReports() {
  try {
    const response = await fetch(API);
    if (!response.ok) throw new Error("Backend unavailable");
    reports = await response.json();
    note.textContent = `${reports.length} report(s) loaded from the C backend.`;
    render();
  } catch (err) {
    grid.innerHTML = '<div class="empty">The C backend is not connected yet. Follow the README setup steps to run this website through Apache CGI.</div>';
    note.textContent = "Backend connection required.";
  }
}
document.querySelectorAll(".filter").forEach(button => button.addEventListener("click", () => {
  document.querySelectorAll(".filter").forEach(b => b.classList.remove("active"));
  button.classList.add("active"); activeFilter = button.dataset.filter; render();
}));
search.addEventListener("input", render);
document.getElementById("report-form").addEventListener("submit", async event => {
  event.preventDefault();
  const form = event.currentTarget, message = document.getElementById("form-message");
  const data = new URLSearchParams(new FormData(form));
  message.className = "form-message"; message.textContent = "Submitting...";
  try {
    const response = await fetch(API, {method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:data.toString()});
    const result = await response.json();
    if (!response.ok || !result.success) throw new Error(result.error || "Could not save report.");
    message.textContent = "Report submitted successfully!";
    form.reset(); await loadReports();
  } catch (err) { message.className = "form-message error"; message.textContent = err.message + " Check the C backend setup."; }
});
const dateInput = document.querySelector('input[name="date"]');
dateInput.value = new Date().toISOString().slice(0,10);
loadReports();
