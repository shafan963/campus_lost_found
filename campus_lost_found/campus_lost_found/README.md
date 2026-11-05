# CampusFind — College Campus Lost & Found Register

A simple college project with an HTML/CSS/JavaScript frontend and a **C language CGI backend**. Students and staff can post lost/found reports and search the campus board.

## Project files
- `index.html` — website layout
- `style.css` — responsive design
- `app.js` — search, filters and form submission
- `cgi-bin/lostfound.c` — C backend (reads and writes report records)
- `data/reports.txt` — created automatically by you; stores reports

## Important: how C works with a website
A browser does not display a C program directly. HTML/CSS creates the pages, JavaScript sends form requests, and the C program runs on the server as a CGI application to save and return reports.

## Run with Apache CGI (Linux / Ubuntu)
1. Install Apache and GCC:
   ```bash
   sudo apt update
   sudo apt install apache2 build-essential
   ```
2. Enable CGI:
   ```bash
   sudo a2enmod cgi
   sudo systemctl restart apache2
   ```
3. Copy the project into Apache's web folder (example):
   ```bash
   sudo cp -r campus_lost_found /var/www/html/campusfind
   ```
4. Create the data folder and allow Apache to write:
   ```bash
   sudo mkdir -p /var/www/html/campusfind/data
   sudo touch /var/www/html/campusfind/data/reports.txt
   sudo chown -R www-data:www-data /var/www/html/campusfind/data
   ```
5. Compile the C file:
   ```bash
   cd /var/www/html/campusfind/cgi-bin
   sudo gcc lostfound.c -o lostfound.cgi
   sudo chmod 755 lostfound.cgi
   ```
6. Configure Apache so `.cgi` files in this folder execute. Add this inside an appropriate Apache config / virtual host:
   ```apache
   ScriptAlias /campusfind/cgi-bin/ /var/www/html/campusfind/cgi-bin/
   <Directory "/var/www/html/campusfind/cgi-bin">
       Options +ExecCGI
       Require all granted
   </Directory>
   ```
   Restart Apache:
   ```bash
   sudo systemctl restart apache2
   ```
7. Open `http://localhost/campusfind/` in your browser.

## Notes
- This is an educational prototype, not a production-ready system.
- The sample backend stores records in a plain text file. For real deployment, add authentication, input validation, moderation, HTTPS, privacy protections and a database.
- In this classroom demo, the contact field is shown on the public item card. For a real campus deployment, do not expose phone numbers or personal email addresses publicly; add authentication and a private contact/reveal workflow.
