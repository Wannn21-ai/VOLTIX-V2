# Checklist VOLTIX V2 — Presentasi 2–3 Hari

## Hari 1 — perangkat tegangan rendah

- [ ] Build firmware PlatformIO berhasil tanpa error.
- [ ] Upload melalui USB dan buka Serial Monitor pada 115200 baud.
- [ ] Saat reset/boot, relay terbukti berada pada posisi OFF.
- [ ] OLED menampilkan menu `Online / Offline / Reset System`.
- [ ] NAV berpindah pilihan dan OK mengonfirmasi pilihan.
- [ ] LED Wi-Fi, LED hijau, LED merah, buzzer, dan relay diuji dari sisi logika.
- [ ] Mode online tanpa kredensial memunculkan jaringan `VOLTIX-SETUP`.
- [ ] Captive portal menyimpan Wi-Fi 2.4 GHz dan OLED menampilkan status.
- [ ] Dashboard terbuka tanpa login dan menunjukkan `Alat online`.

Jangan hubungkan atau menangani sisi listrik PLN pada tahap ini.

## Hari 2 — integrasi terawasi

Bagian listrik tegangan tinggi hanya ditangani guru/teknisi dewasa yang kompeten
dengan enclosure dan proteksi yang sesuai. Pengguna melakukan pemeriksaan dari
dashboard/antarmuka, bukan menyentuh sambungan.

- [ ] PZEM memberikan sampel valid dan angka masuk akal menurut alat pembanding.
- [ ] START membuat status `VALIDATING_LOAD`, lalu `MONITORING` jika beban valid.
- [ ] STOP mengubah relay ke OFF dan menambahkan satu histori.
- [ ] Cabut Wi-Fi saja: monitoring tetap berjalan dan histori menjadi pending.
- [ ] Sambungkan Wi-Fi kembali: pending history turun dan histori muncul di web.
- [ ] Mode Offline menyelesaikan satu sesi lalu menampilkan `Next Device`.
- [ ] Batas overload diuji dengan metode aman yang dipilih guru/teknisi; relay OFF,
      alasan `OVERLOAD` tersimpan, dan alarm baru berhenti setelah tombol OK.
- [ ] Restart di tengah sesi diuji: checkpoint ditemukan dan sesi dipulihkan.

## Hari 3 — gladi presentasi

- [ ] Bersihkan data latihan yang tidak diperlukan.
- [ ] Siapkan satu beban demo yang sudah dinyatakan aman oleh pendamping.
- [ ] Gunakan hotspot/Wi-Fi yang sudah diuji dan siapkan koneksi cadangan.
- [ ] Buka tab Dashboard, Histori, Advanced, dan Pengaturan sebelum presentasi.
- [ ] Demonstrasikan alur: Online → START → real-time → STOP → Histori.
- [ ] Demonstrasikan offline/sinkronisasi dan proteksi hanya jika skenario uji sudah
      disetujui pendamping.
- [ ] Jelaskan bahwa ESP32 adalah sumber kebenaran dan Firebase adalah sinkronisasi.

## Kriteria siap presentasi

VOLTIX dianggap siap jika semua fungsi wajib lulus dua kali berturut-turut tanpa
reset tak terduga, relay selalu OFF saat boot/error, dan skenario listrik sudah
disetujui pendamping yang kompeten.
