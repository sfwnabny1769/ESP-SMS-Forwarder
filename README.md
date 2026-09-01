# SMS Gateway Backend (ESP32 + SIM800L) - Laravel 12

Sistem Backend SMS Gateway berbasis **Laravel 12 (PHP 8.3)** & **Bootstrap 5** yang dirancang untuk menerima, menyimpan, dan mengelola SMS dari modul **ESP32-S3 + SIM800L** melalui REST API yang aman dan terstruktur serta dilengkapi **Admin Dashboard Terproteksi**.

---

## 📌 Fitur Utama

1. **Keamanan Admin Dashboard (Session Auth & Hardened)**:
   - Autentikasi Admin berbasis sesi dengan proteksi **Anti Brute-Force Rate Limiting** (maksimal 5 percobaan gagal per menit).
   - Proteksi **Anti Session-Fixation** (`session()->regenerate()`) dan token **CSRF** pada seluruh form & aksi logout.
   - Hak akses eksklusif untuk admin terdaftar (seeder awal).
2. **Device Authentication (IoT REST API)**:
   - Keamanan API modul ESP32 berbasis **Device Token** unik (HTTP Header `X-Device-Token` / Bearer / JSON Body).
   - Middleware `EnsureValidDeviceToken` yang memvalidasi setiap request IoT.
3. **REST API Gateway (IoT Endpoints)**:
   - Heartbeat status perangkat (`POST /api/device/heartbeat`).
   - Respon eksekusi AT Command (`POST /api/device/command-response`).
   - Pengiriman & Ingest SMS Masuk (`POST /api/sms`).
4. **Pemantauan Status SIM Card & Jaringan Realtime**:
   - Status Kartu SIM: `READY` / `UNKNOWN` / `SIM PIN`.
   - Registrasi Sinyal Jaringan Seluler (`AT+CREG?`): `+CREG: 0,1` (Registered Home), `+CREG: 0,5` (Registered Roaming), `+CREG: 0,2` (Searching).
   - Kuat Sinyal SIM800L (CSQ 0-31, estimasi dBm & Kualitas Sinyal: Sangat Baik / Baik / Cukup / Lemah).
   - Operator Seluler (`AT+COPS?`).
   - Last Seen Activity Tracker.
5. **Penarikan Pesan dari Memori SIM (SIM Storage Sync)**:
   - Tombol **"Tarik Pesan dari SIM"** pada Web Dashboard & Device Console.
   - Fitur otomatis penarikan seluruh SMS tersimpan di memori SIM (`AT+CMGL="ALL"`) saat modul pertama kali terhubung ke jaringan maupun melalui trigger jarak jauh dari Web.
6. **Interactive GSM Live Console & AT Terminal**:
   - Eksekusi AT Command secara remote dari Web ke ESP32 + SIM800L.
   - Preset AT Command instan: `SYNC_SIM_SMS`, `AT+CMGL="ALL"`, `AT+CPMS?`, `AT+CREG?`, `AT+CSQ`, `AT+CPIN?`, `AT+COPS?`, `AT+CBC`.
7. **Manajemen Data SMS**:
   - Tabel SMS dengan pencarian live, filter per device, filter status diproses, dan pagination Bootstrap 5.
8. **CRUD Devices**:
   - Manajemen perangkat ESP32, penambahan device, update status, hapus, dan tombol **Regenerate Token**.

---

## 📂 Struktur Folder Proyek

```text
app/
├── Http/
│   ├── Controllers/
│   │   ├── Api/
│   │   │   ├── DeviceController.php     # Endpoint POST /api/device/heartbeat & /api/device/command-response
│   │   │   └── SmsController.php        # Endpoint POST /api/sms
│   │   └── Web/
│   │       ├── AuthController.php       # Controller Login, Logout & Rate Limiting
│   │       ├── DashboardController.php  # Controller halaman utama Dashboard
│   │       ├── DeviceController.php     # CRUD Perangkat ESP32, syncSimSms & Live AT Console
│   │       └── SmsController.php        # List, Detail, Toggle, Delete SMS & syncFromSim
│   ├── Middleware/
│   │   └── EnsureValidDeviceToken.php   # Otorisasi token device pada API ESP32
│   ├── Requests/
│   │   ├── Api/
│   │   │   ├── DeviceHeartbeatRequest.php
│   │   │   └── StoreSmsRequest.php
│   │   └── Web/
│   │       ├── StoreDeviceRequest.php
│   │       └── UpdateDeviceRequest.php
│   └── Resources/
│       ├── DeviceResource.php           # API Transformer untuk Device
│       └── SmsResource.php              # API Transformer untuk SMS
├── Models/
│   ├── Device.php                       # Model Device & Relasi
│   ├── Sms.php                          # Model SMS, Scope Search & Filter
│   └── User.php                         # Model Admin User
└── Services/
    ├── DeviceService.php                # Service Logic Device, Heartbeat & AT Queue
    └── SmsService.php                   # Service Logic SMS & Statistics

src/
└── main.cpp                             # Firmware ESP32-S3: Tri-Mode Gateway Orchestrator

lib/
├── ApiClient/                           # HTTP Client ESP32 untuk REST API Laravel
├── GSMManager/                          # Driver SIM800L: AT Command State Machine & Parser
├── TelegramClient/                      # Direct Telegram HTTPS Client (Mode 1 Standalone)
└── WifiManagerCustom/                   # On-Chip Web Portal (Mode 2) & NVS Storage Manager

database/
├── migrations/
│   ├── 0001_01_01_000000_create_users_table.php
│   ├── 2026_08_07_000001_create_devices_table.php
│   ├── 2026_08_07_000002_create_sms_table.php
│   └── 2026_08_07_000003_add_gsm_debug_columns_to_devices_table.php
└── seeders/
    ├── DatabaseSeeder.php
    ├── DeviceSeeder.php
    ├── SmsSeeder.php
    └── UserSeeder.php                   # Default Admin Seeder

public/
├── css/
│   └── auth.css                         # Dark theme CSS untuk halaman login
└── js/

resources/views/
├── auth/
│   └── login.blade.php                  # Halaman Login Admin
├── layouts/
│   └── app.blade.php                    # Master Layout Bootstrap 5, Sidebar & Navbar Profile
├── dashboard.blade.php                  # Dashboard Statistik & Chart
├── devices/
│   └── index.blade.php                  # CRUD Devices, Modal Token & Live AT Console
└── sms/
    └── index.blade.php                  # Inbox SMS, Search, Filter & Modal Detail

routes/
├── api.php                              # REST API Routes (Khusus ESP32 dengan token)
└── web.php                              # Web Dashboard Routes (Terproteksi auth)
```

---

## 🛠️ Instalasi & Konfigurasi

### 1. Requirements
- **PHP 8.3** atau lebih baru
- **Composer 2.x**
- **MySQL** / MariaDB / SQLite

### 2. Langkah Instalasi

```bash
# Clone repository & masuk ke direktori
git clone <repository_url>
cd SMS_Forwarder

# Install Dependensi Composer
composer install

# Salin environment file & setting Database
cp .env.example .env

# Generate Application Key
php artisan key:generate
```

### 3. Konfigurasi Database `.env` (MySQL)

```env
DB_CONNECTION=mysql
DB_HOST=127.0.0.1
DB_PORT=3306
DB_DATABASE=sms_gateway
DB_USERNAME=root
DB_PASSWORD=
```

### 4. Jalankan Migration & Seeder

```bash
# Jalankan migration database beserta akun admin & device seeder awal
php artisan migrate:fresh --seed
```

> **Kredensial Default Admin:**
> - **URL:** `http://127.0.0.1:8000/login`
> - **Email:** `abin.shafwan@gmail.com`
> - **Password:** `Teti1769`

### 5. Jalankan Local Server

```bash
php artisan serve
```

Buka browser dan akses: `http://127.0.0.1:8000` *(akan otomatis diarahkan ke halaman login jika belum terautentikasi)*.

---

## 🔌 Dokumentasi REST API (ESP32 IoT Gateway)

Semua endpoint IoT wajib menyertakan token perangkat pada HTTP Header `X-Device-Token` atau field JSON `token`.

### 1. Heartbeat Device (`POST /api/device/heartbeat`)

Dikirimkan oleh ESP32 secara berkala (setiap 30-60 detik) untuk mengabarkan status sinyal, operator, dan mengambil antrean AT Command.

- **Headers**:
  - `Content-Type: application/json`
  - `X-Device-Token: <TOKEN_DEVICE>`

- **Request Body**:
```json
{
  "token": "ESP32_DEFAULT_SECRET_TOKEN_12345",
  "signal": 28,
  "operator": "TELKOMSEL",
  "sim_status": "READY",
  "reg_status": "Registered Home (+CREG: 0,1)"
}
```

- **Response Success (200 OK)**:
```json
{
  "success": true,
  "message": "Heartbeat received successfully.",
  "data": {
    "id": 1,
    "name": "ESP32-Gateway-Node01",
    "status": "online",
    "is_online": true,
    "signal": 28,
    "operator": "TELKOMSEL",
    "sim_status": "READY",
    "reg_status": "Registered Home (+CREG: 0,1)",
    "pending_command": null,
    "last_seen": "2026-09-01 15:00:00",
    "last_seen_human": "1 second ago"
  }
}
```

---

### 2. Forward Incoming SMS (`POST /api/sms`)

Dikirimkan oleh ESP32 saat menerima SMS baru dari modul SIM800L.

- **Headers**:
  - `Content-Type: application/json`
  - `X-Device-Token: <TOKEN_DEVICE>`

- **Request Body**:
```json
{
  "token": "ESP32_DEFAULT_SECRET_TOKEN_12345",
  "phone": "+6281234567890",
  "message": "Kode verifikasi Anda adalah 492019. Jangan beritahukan kepada siapapun.",
  "received_at": "2026-09-01 15:05:00"
}
```

- **Response Success (201 Created)**:
```json
{
  "success": true,
  "message": "SMS saved successfully.",
  "data": {
    "id": 101,
    "device_id": 1,
    "device_name": "ESP32-Gateway-Node01",
    "phone": "+6281234567890",
    "message": "Kode verifikasi Anda adalah 492019. Jangan beritahukan kepada siapapun.",
    "received_at": "2026-09-01 15:05:00",
    "processed": false
  }
}
```

---

### 3. Send Command Response (`POST /api/device/command-response`)

Dikirimkan oleh ESP32 setelah mengeksekusi AT Command yang diminta oleh Web Admin Console.

- **Headers**:
  - `Content-Type: application/json`
  - `X-Device-Token: <TOKEN_DEVICE>`

- **Request Body**:
```json
{
  "token": "ESP32_DEFAULT_SECRET_TOKEN_12345",
  "command": "AT+CSQ",
  "response": "+CSQ: 28,0\n\nOK"
}
```

- **Response Success (200 OK)**:
```json
{
  "success": true,
  "message": "Command response updated successfully.",
  "data": {
    "id": 1,
    "name": "ESP32-Gateway-Node01",
    "pending_command": null,
    "command_response": "+CSQ: 28,0\n\nOK",
    "command_updated_at": "2026-09-01 15:06:00"
  }
}
```

---

## 🧪 Automated Testing

Jalankan pengujian otomatis (Unit & Feature Tests):

```bash
# Jalankan seluruh test suite
php artisan test

# Jalankan khusus pengujian keamanan autentikasi
php artisan test --filter=AuthTest
```

---

## 🌐 Panduan Deployment & Simulasi Remote Cloud

Untuk menguji komunikasi ESP32 ke backend Laravel melalui internet publik (HTTPS):

### 1. Rekomendasi: Gunakan Cloudflare Tunnel (Quick Tunnel)
* **Mengapa Cloudflare Tunnel?**  
  Cloudflare Tunnel menggunakan sertifikat SSL RSA standar yang ringan (~1.5 KB) dan bebas dari halaman peringatan (*interstitial warning page*), sehingga sangat ramah untuk buffer memori TLS mikrokontroler ESP32 (`mbedTLS`).
* **Cara Menjalankan:**
  ```bash
  # Terminal 1 (Laravel Server):
  php artisan serve --port=8000

  # Terminal 2 (Cloudflare Quick Tunnel):
  npx untun tunnel --port 8000
  # atau
  cloudflared tunnel --url http://localhost:8000
  ```
* Masukkan URL yang dihasilkan (`https://xxxx.trycloudflare.com`) ke Web Config Portal ESP32 (`192.168.4.1`).

### 2. Catatan Khusus Mengenai Ngrok Free Tier
* **Batasan Teknis Ngrok Free:**  
  Ngrok Free Tier menggunakan sertifikat *Wildcard ECDSA P-384* dengan ukuran rantai sertifikat besar (~4.5 KB) serta proteksi *warning interstitial*. Pada mikrokontroler ESP32 dengan alokasi buffer mbedTLS default (4 KB), jabat tangan TLS ke domain `*.ngrok-free.dev` / `*.ngrok-free.app` dapat mengalami `MBEDTLS_ERR_SSL_CONN_EOF (-29312)`.
* **Solusi Produksi:** Di server nyata (*production* VPS / PaaS seperti Railway, Render, DigitalOcean), gunakan sertifikat SSL standar (Let's Encrypt / DigiCert) yang didukung penuh secara *out-of-the-box* oleh ESP32.

---

## 📝 License
Proyek ini dikembangkan dengan lisensi MIT.

