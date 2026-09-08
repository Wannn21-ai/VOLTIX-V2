# VOLTIX V2

VOLTIX V2 adalah smart energy monitor satu kanal berbasis ESP32, PZEM-004T v4,
relay, OLED, LED, buzzer, Firebase Realtime Database, dan dashboard web tanpa
login pengguna.

## Fitur versi presentasi

- Tanpa login, registrasi, pairing, atau pilihan banyak alat.
- Satu alat menangani satu beban pada satu waktu.
- Monitoring tegangan, arus, daya, energi, frekuensi, faktor daya, durasi, dan biaya.
- START/STOP online dengan konfirmasi perintah dari ESP32.
- Mode offline lokal dan alur `Next Device` melalui OLED/tombol.
- Histori local-first yang disinkronkan otomatis saat koneksi kembali.
- Proteksi overload: relay langsung OFF, histori beralasan `OVERLOAD`, LED merah
  dan buzzer terkunci sampai tombol OK ditekan.
- Checkpoint sesi dan pemulihan setelah ESP32 restart/padam.
- Captive portal `VOLTIX-SETUP` untuk Wi-Fi 2.4 GHz.

## Struktur proyek

- `firmware/` — proyek PlatformIO untuk ESP32.
- `web/` — dashboard statis dengan desain VOLTIX lama.
- `firebase/` — skema contoh dan rules Realtime Database.
- `dist/` — hasil `npm run build:web` untuk deployment.

## Prinsip sistem

- ESP32 adalah sumber kebenaran untuk mode, session, relay, dan alarm.
- Web hanya mengirim perintah dan menampilkan konfirmasi dari ESP32.
- Monitoring tetap berjalan saat Wi-Fi terputus.
- Histori disimpan lokal lebih dahulu lalu disinkronkan saat ONLINE.
- Satu alat mengukur satu beban pada satu waktu dan mendukung `Next Device`.

## Perintah singkat

```bash
npm run build:web
```

Untuk firmware, buka folder `firmware/` sebagai proyek PlatformIO lalu jalankan:

```bash
pio run -d firmware
pio run -d firmware -t upload
```

Library PZEM sengaja menggunakan `mandulaj/PZEM-004T-v30`, sesuai library yang
sudah pernah berhasil pada unit PZEM-004T fisik versi v4 milik proyek ini.

## Urutan penggunaan

1. Saat boot, relay selalu dipaksa OFF sebelum komponen lain dimulai.
2. Jika ada checkpoint, VOLTIX memulihkan sesi lebih dahulu.
3. Jika tidak, pilih `Online`, `Offline`, atau `Reset System` pada OLED dengan
   tombol NAV dan OK.
4. Pada mode online pertama kali, sambungkan ponsel/laptop ke `VOLTIX-SETUP`,
   lalu buka `192.168.4.1` untuk menyimpan Wi-Fi.
5. START/STOP online dilakukan dari dashboard. Mode offline dimulai dari alat.

## Keselamatan pengujian

Lakukan pemeriksaan pertama hanya pada ESP32, OLED, tombol, LED, buzzer, dan
logika relay dengan sisi listrik tegangan tinggi tetap tidak terhubung. Penanganan
terminal listrik PLN, beban, enclosure, sekring, dan isolasi harus dilakukan oleh
guru/teknisi dewasa yang kompeten; pengguna cukup mengamati dashboard dan hasil
uji. Jangan menyentuh atau mengubah sambungan listrik saat sistem diberi daya.

Lihat `docs/PRESENTATION_CHECKLIST.md` untuk urutan verifikasi dua hari.
