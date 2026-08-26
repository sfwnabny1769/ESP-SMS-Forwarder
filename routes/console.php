<?php

use App\Services\TelegramService;
use Illuminate\Foundation\Inspiring;
use Illuminate\Support\Facades\Artisan;
use Illuminate\Support\Facades\Http;

Artisan::command('inspire', function () {
    $this->comment(Inspiring::quote());
})->purpose('Display an inspiring quote');

Artisan::command('telegram:test', function () {
    $token = config('services.telegram.bot_token');
    $chatId = config('services.telegram.chat_id');

    $this->info("=== Menguji Koneksi Bot Telegram ===");
    $this->line("Bot Token : " . ($token ? substr($token, 0, 10) . '...' : '[KOSONG]'));
    $this->line("Chat ID   : " . ($chatId ?? '[KOSONG]'));

    if (empty($token) || empty($chatId)) {
        $this->error("Error: TELEGRAM_BOT_TOKEN atau TELEGRAM_CHAT_ID masih kosong di .env!");
        return 1;
    }

    $url = "https://api.telegram.org/bot{$token}/sendMessage";
    $this->line("Menghubungi: {$url} ...");

    try {
        $response = Http::timeout(10)->post($url, [
            'chat_id' => $chatId,
            'text' => "🔔 <b>Tes Koneksi SMS Gateway Bot Berhasil!</b>\n\nNotifikasi SMS akan otomatis diteruskan ke sini.",
            'parse_mode' => 'HTML',
        ]);

        $this->line("HTTP Status: " . $response->status());
        $this->line("Response Body: " . $response->body());

        if ($response->successful()) {
            $this->info("✅ Sukses! Pesan berhasil masuk ke akun/grup Telegram Anda.");
            return 0;
        } else {
            $this->error("❌ Gagal mengirim pesan ke Telegram.");
            return 1;
        }
    } catch (\Throwable $e) {
        $this->error("❌ Terjadi Exception: " . $e->getMessage());
        return 1;
    }
})->purpose('Menguji pengiriman notifikasi ke Telegram');
