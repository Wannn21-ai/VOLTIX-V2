# Firebase Realtime Database

Project: `voltv1`
Region: Asia Southeast 1 (Singapore)
Root data: `/voltix`

## Struktur data

- `/voltix/live`: status dan pembacaan terakhir yang dikirim ESP32.
- `/voltix/command`: satu perintah web yang menunggu diproses ESP32.
- `/voltix/ack`: konfirmasi ESP32 atas perintah terakhir.
- `/voltix/config`: tarif dan batas proteksi yang dapat diubah saat sesi idle.
- `/voltix/history/{sessionId}`: hasil akhir sesi.
- `/voltix/events/{eventId}`: log kejadian penting.

`database.rules.json` mempertahankan akses tanpa akun hanya sampai 6 Oktober 2026,
sesuai batas mode presentasi. Setelah presentasi, backend perlu autentikasi perangkat
atau aturan yang lebih ketat sebelum dipakai di luar lingkungan demo.

`initial-data.json` adalah data awal opsional. Impor hanya ke database baru/kosong
karena proses impor dari Firebase Console dapat mengganti data pada lokasi tujuan.
