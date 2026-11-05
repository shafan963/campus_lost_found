
const API = "cgi-bin/lostfound.cgi";

function escapeHTML(value = "") {
    return String(value).replace(/[&<>"']/g, c => ({
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;"
    })[c]);
}

async function postData(data) {
    const response = await fetch(API, {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body: data.toString(),
        credentials: "same-origin",
        cache: "no-store"
    });

    const result = await response.json();

    if (!response.ok || !result.success) {
        throw new Error(result.error || "Request failed.");
    }

    return result;
}

// LOAD REPORTS
async function loadDashboard() {
    const container = document.getElementById("admin-reports");

    try {
        const response = await fetch(API, {
            credentials: "same-origin",
            cache: "no-store"
        });

        if ([400, 401, 403].includes(response.status)) {
            location.href = "login.html";
            return;
        }

        if (!response.ok) throw new Error("Could not load reports.");

        const reports = await response.json();

        document.getElementById("total-reports").textContent = reports.length;
        document.getElementById("lost-reports").textContent =
            reports.filter(r => r.type === "Lost").length;
        document.getElementById("found-reports").textContent =
            reports.filter(r => r.type === "Found").length;

        container.innerHTML = reports.length
            ? reports.map(r => `
                <article class="item-card">
                    <h3>${escapeHTML(r.item)}</h3>
                    <p>Type: ${escapeHTML(r.type)}</p>
                    <p>Category: ${escapeHTML(r.category)}</p>
                    <p>Location: ${escapeHTML(r.location)}</p>
                    <p>Date: ${escapeHTML(r.date)}</p>
                    <p>Description: ${escapeHTML(r.description)}</p>
                    <p>Reported by: ${escapeHTML(r.reporter)}</p>
                    <p>Contact: ${escapeHTML(r.contact)}</p>
                </article>
            `).join("")
            : "No reports available.";

    } catch (error) {
        container.textContent = error.message;
    }
}

// LOGOUT
document.getElementById("logout-btn").addEventListener("click", async () => {
    const data = new URLSearchParams({ action: "admin_logout" });

    try {
        await postData(data);
        location.href = "login.html";
    } catch (error) {
        alert(error.message);
    }
});

// CREATE ACCOUNT
document.getElementById("create-account-form")
    .addEventListener("submit", async event => {
        event.preventDefault();

        const form = event.currentTarget;
        const message = document.getElementById("account-message");
        const data = new URLSearchParams(new FormData(form));
        data.append("action", "create_account");

        message.textContent = "Creating account...";

        try {
            await postData(data);
            message.style.color = "green";
            message.textContent = "Account created successfully!";
            form.reset();
            await loadUsers();
        } catch (error) {
            message.style.color = "#c0392b";
            message.textContent = error.message;
        }
    });

// LOAD USERS
let allUsers = [];

async function loadUsers() {
    const container = document.getElementById("users-list");
    container.textContent = "Loading users...";

    try {
        const data = new URLSearchParams({ action: "list_users" });
        const response = await fetch(API, {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: data.toString(),
            credentials: "same-origin",
            cache: "no-store"
        });

        if ([400, 401, 403].includes(response.status)) {
            location.href = "login.html";
            return;
        }

        if (!response.ok) throw new Error("Could not load users.");

        allUsers = await response.json();
        renderUsers();

    } catch (error) {
        container.textContent = error.message;
    }
}

// RENDER USERS
function renderUsers() {
    const container = document.getElementById("users-list");
    const search = document.getElementById("user-search")
        .value.trim().toLowerCase();
    const filter = document.getElementById("user-filter").value;

    const filtered = allUsers.filter(user => {
        const matchesRole = filter === "ALL" || user.role === filter;

        const matchesSearch = [
            user.name, user.identifier, user.email,
            user.phone, user.department
        ].join(" ").toLowerCase().includes(search);

        return matchesRole && matchesSearch;
    });

    if (!filtered.length) {
        container.textContent = "No matching users found.";
        return;
    }

    container.innerHTML = filtered.map(user => `
        <article class="item-card">
            <h3>${escapeHTML(user.name || "Name not available")}</h3>
            <p><b>Role:</b> ${escapeHTML(user.role)}</p>
            <p><b>Login ID:</b> ${escapeHTML(user.identifier)}</p>
            <p><b>Email:</b> ${escapeHTML(user.email)}</p>
            <p><b>Phone:</b> ${escapeHTML(user.phone)}</p>
            <p><b>Department:</b> ${escapeHTML(user.department)}</p>
            <p><b>${user.role === "STUDENT" ? "Semester" : "Designation"}:</b>
                ${escapeHTML(user.extra)}</p>
            <p><b>Status:</b> ${escapeHTML(user.status)}</p>

            <button type="button" class="edit-user-btn"
                data-role="${escapeHTML(user.role)}"
                data-id="${escapeHTML(user.identifier)}">
                Edit Details
            </button>

            <button type="button" class="status-user-btn"
                data-role="${escapeHTML(user.role)}"
                data-id="${escapeHTML(user.identifier)}"
                data-status="${user.status === "ACTIVE" ? "DISABLED" : "ACTIVE"}">
                ${user.status === "ACTIVE" ? "Disable Account" : "Enable Account"}
            </button>
        </article>
    `).join("");
}

// ONE CLICK HANDLER FOR USER ACTIONS
document.getElementById("users-list").addEventListener("click", async event => {
    const editButton = event.target.closest(".edit-user-btn");
    const statusButton = event.target.closest(".status-user-btn");

    if (!editButton && !statusButton) return;

    const button = editButton || statusButton;
    const user = allUsers.find(u =>
        u.role === button.dataset.role &&
        u.identifier === button.dataset.id
    );

    if (!user) return;

    if (editButton) {
        const name = prompt("Full name:", user.name);
        if (name === null) return;

        const email = prompt("Email:", user.email);
        if (email === null) return;

        const phone = prompt("Phone:", user.phone);
        if (phone === null) return;

        const department = prompt("Department:", user.department);
        if (department === null) return;

        const label = user.role === "STUDENT" ? "Semester" : "Designation";
        const extra = prompt(label + ":", user.extra);
        if (extra === null) return;

        const data = new URLSearchParams({
            action: "edit_user",
            role: user.role,
            identifier: user.identifier,
            full_name: name,
            email,
            phone,
            department,
            extra
        });

        try {
            await postData(data);
            alert("User details updated!");
            await loadUsers();
        } catch (error) {
            alert(error.message);
        }
    }

    if (statusButton) {
        const status = button.dataset.status;
        const actionText = status === "DISABLED" ? "disable" : "enable";

        if (!confirm(`Are you sure you want to ${actionText} this account?`)) {
            return;
        }

        const data = new URLSearchParams({
            action: "set_user_status",
            role: user.role,
            identifier: user.identifier,
            status
        });

        try {
            await postData(data);
            alert("Account status updated!");
            await loadUsers();
        } catch (error) {
            alert(error.message);
        }
    }
});

// SEARCH AND FILTER
document.getElementById("user-search").addEventListener("input", renderUsers);
document.getElementById("user-filter").addEventListener("change", renderUsers);

// INITIAL LOAD
loadDashboard();
loadUsers();
