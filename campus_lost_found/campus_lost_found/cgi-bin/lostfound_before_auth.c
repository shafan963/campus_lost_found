/*
 * CampusFind - C CGI backend
 * GET  /cgi-bin/lostfound.cgi  -> JSON array of reports
 * POST /cgi-bin/lostfound.cgi -> append a report to data/reports.txt
 *
 * Build on Linux: gcc lostfound.c -o lostfound.cgi
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <bcrypt.h>

#define DATA_FILE "E:/campus_lost_found/data/reports.txt"
#define USERS_FILE "E:/campus_lost_found/data/users.txt"
#define SESSIONS_FILE "E:/campus_lost_found/data/sessions.txt"
#define SALT_SIZE 16
#define HASH_SIZE 32
#define ITERATIONS 100000
#define MAX_BODY 8192
#define FIELD 512

static void json_string(const char *s) {
    putchar('"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '\t') printf("\\t");
        else if (c >= 32) putchar(c);
    }
    putchar('"');
}
static void decode(char *out, const char *in, size_t cap) {
    size_t j = 0;
    while (*in && j + 1 < cap) {
        if (*in == '+') { out[j++] = ' '; in++; }
        else if (*in == '%' && isxdigit((unsigned char)in[1]) && isxdigit((unsigned char)in[2])) {
            char hex[3] = {in[1], in[2], 0}; out[j++] = (char)strtol(hex, NULL, 16); in += 3;
        } else out[j++] = *in++;
    }
    out[j] = '\0';
}
static void get_field(const char *body, const char *key, char *out, size_t cap) {
    char copy[MAX_BODY], *save = NULL, *part;
    snprintf(copy, sizeof(copy), "%s", body);
    out[0] = '\0';
    for (part = strtok_r(copy, "&", &save); part; part = strtok_r(NULL, "&", &save)) {
        char *eq = strchr(part, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(part, key) == 0) { decode(out, eq + 1, cap); return; }
    }
}
static void clean_field(char *s) {
    for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r' || *s == '\t') *s = ' ';
}
static void output_json(void) {
    FILE *fp = fopen(DATA_FILE, "r");
    char line[FIELD * 7];
    int first = 1;
    printf("Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n\r\n[");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            char *fields[8];
            int i;
            line[strcspn(line, "\r\n")] = '\0';
            fields[0] = line;
            for (i = 1; i < 8; i++) {
                char *separator = strchr(fields[i - 1], '|');
                if (!separator) break;
                *separator = '\0';
                fields[i] = separator + 1;
            }
            if (i != 8 || strchr(fields[7], '|')) continue;
            if (!first) putchar(',');
            first = 0;
            printf("{\"type\":"); json_string(fields[0]);
            printf(",\"item\":"); json_string(fields[1]);
            printf(",\"category\":"); json_string(fields[2]);
            printf(",\"date\":"); json_string(fields[3]);
            printf(",\"location\":"); json_string(fields[4]);
            printf(",\"description\":"); json_string(fields[5]);
            printf(",\"reporter\":"); json_string(fields[6]);
            printf(",\"contact\":"); json_string(fields[7]); printf("}");
        }
        fclose(fp);
    }
    printf("]");
}
static void output_error(const char *message) {
    printf("Status: 400 Bad Request\r\nContent-Type: application/json\r\n\r\n{\"success\":false,\"error\":");
    json_string(message); printf("}");
}
static int hash_password(const char *password,
                         const unsigned char *salt,
                         unsigned char *hash) {
    BCRYPT_ALG_HANDLE algorithm = NULL;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        NULL,
        BCRYPT_ALG_HANDLE_HMAC_FLAG
    );

    if (status < 0) return 0;

    status = BCryptDeriveKeyPBKDF2(
        algorithm,
        (PUCHAR)password,
        (ULONG)strlen(password),
        (PUCHAR)salt,
        SALT_SIZE,
        ITERATIONS,
        hash,
        HASH_SIZE,
        0
    );

    BCryptCloseAlgorithmProvider(algorithm, 0);

    return status >= 0;
}
static int generate_salt(unsigned char *salt) {
    NTSTATUS status = BCryptGenRandom(
        NULL,
        salt,
        SALT_SIZE,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    return status >= 0;
}
static void to_hex(const unsigned char *input, size_t length, char *output) {
    static const char digits[] = "0123456789abcdef";

    for (size_t i = 0; i < length; i++) {
        output[i * 2] = digits[input[i] >> 4];
        output[i * 2 + 1] = digits[input[i] & 0x0F];
    }

    output[length * 2] = '\0';
}
static int from_hex(const char *input, unsigned char *output, size_t length) {
    for (size_t i = 0; i < length; i++) {
        unsigned int value;

        if (sscanf(input + i * 2, "%2x", &value) != 1)
            return 0;

        output[i] = (unsigned char)value;
    }

    return input[length * 2] == '\0';
}
static int verify_password(const char *password,
                           const unsigned char *salt,
                           const unsigned char *expected_hash) {
    unsigned char actual_hash[HASH_SIZE];

    if (!hash_password(password, salt, actual_hash))
        return 0;

    unsigned char difference = 0;

    for (int i = 0; i < HASH_SIZE; i++)
        difference |= actual_hash[i] ^ expected_hash[i];

    return difference == 0;
}
static int create_admin(const char *username, const char *password) {
    const char *lock_file =
        "E:/campus_lost_found/data/admin_setup.lock";

    HANDLE lock = CreateFileA(
        lock_file,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_HIDDEN,
        NULL
    );

    if (lock == INVALID_HANDLE_VALUE)
        return 0;

    CloseHandle(lock);

    FILE *fp = fopen(USERS_FILE, "a+");
    if (!fp) {
        DeleteFileA(lock_file);
        return 0;
    }

    rewind(fp);
    char line[2048];

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "ADMIN|", 6) == 0) {
            fclose(fp);
            DeleteFileA(lock_file);
            return 0;
        }
    }

    unsigned char salt[SALT_SIZE], hash[HASH_SIZE];
    char salt_hex[SALT_SIZE * 2 + 1];
    char hash_hex[HASH_SIZE * 2 + 1];

    if (!generate_salt(salt) ||
        !hash_password(password, salt, hash)) {
        fclose(fp);
        DeleteFileA(lock_file);
        return 0;
    }

    to_hex(salt, SALT_SIZE, salt_hex);
    to_hex(hash, HASH_SIZE, hash_hex);

    int written = fprintf(
        fp, "ADMIN|%s|%s|%s|ACTIVE\n",
        username, salt_hex, hash_hex
    );

    int closed = fclose(fp);

    if (written < 0 || closed != 0) {
        DeleteFileA(lock_file);
        return 0;
    }

    return 1;
}
int main(void) {
    const char *method = getenv("REQUEST_METHOD");
    if (method && strcmp(method, "POST") == 0) {
        char body[MAX_BODY] = {0}, type[FIELD], item[FIELD], category[FIELD], date[FIELD];
        char location[FIELD], description[FIELD], reporter[FIELD], contact[FIELD];
        const char *lenstr = getenv("CONTENT_LENGTH");
        int len = lenstr ? atoi(lenstr) : 0;
        if (len <= 0 || len >= MAX_BODY) { output_error("Invalid form data."); return 0; }
        if (fread(body, 1, (size_t)len, stdin) != (size_t)len) { output_error("Could not read form data."); return 0; }
        char action[FIELD];
get_field(body, "action", action, sizeof(action));
if (strcmp(action, "setup_admin") == 0) {
    const char *remote = getenv("REMOTE_ADDR");

if (!remote ||
    (strcmp(remote, "127.0.0.1") != 0 &&
     strcmp(remote, "::1") != 0)) {
    output_error("Admin setup is allowed only from this computer.");
    return 0;
}
    char username[FIELD], password[FIELD], confirm[FIELD];

    get_field(body, "username", username, sizeof(username));
    get_field(body, "password", password, sizeof(password));
    get_field(body, "confirm_password", confirm, sizeof(confirm));

   int valid_username = 1;

if (strlen(username) < 4 || strlen(username) >= FIELD)
    valid_username = 0;

for (size_t i = 0; username[i]; i++) {
    if (!isalnum((unsigned char)username[i]) &&
        username[i] != '_' && username[i] != '-') {
        valid_username = 0;
    }
}

if (!valid_username ||
    strlen(password) < 12 ||
    strlen(password) >= FIELD ||
    strcmp(password, confirm) != 0) {
    output_error("Invalid username or password.");
    return 0;
}

    if (create_admin(username, password)) {
        printf("Content-Type: application/json\r\n\r\n");
        printf("{\"success\":true}");
    } else {
        output_error("Admin setup failed or an admin already exists.");
    }
    return 0;
}
        get_field(body,"type",type,sizeof(type)); get_field(body,"item",item,sizeof(item));
        get_field(body,"category",category,sizeof(category)); get_field(body,"date",date,sizeof(date));
        get_field(body,"location",location,sizeof(location)); get_field(body,"description",description,sizeof(description));
        get_field(body,"reporter",reporter,sizeof(reporter)); get_field(body,"contact",contact,sizeof(contact));
        if (!*item || !*category || !*date || !*location || !*reporter || !*contact ||
            (strcmp(type,"Lost") && strcmp(type,"Found"))) { output_error("Please complete all required fields."); return 0; }
        clean_field(type); clean_field(item); clean_field(category); clean_field(date);
        clean_field(location); clean_field(description); clean_field(reporter); clean_field(contact);
        FILE *fp = fopen(DATA_FILE, "a");
        if (!fp) { output_error("Cannot open data file. Check folder permissions."); return 0; }
        fprintf(fp,"%s|%s|%s|%s|%s|%s|%s|%s\n",type,item,category,date,location,description,reporter,contact);
        fclose(fp);
        printf("Content-Type: application/json\r\n\r\n{\"success\":true}");
        return 0;
    }
    output_json();
    return 0;
}
