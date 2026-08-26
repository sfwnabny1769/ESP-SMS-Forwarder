<?php

namespace App\Services;

use App\Models\Sms;
use Illuminate\Support\Facades\Http;
use Illuminate\Support\Facades\Log;

class TelegramService
{
    protected ?string $botToken;
    protected ?string $chatId;

    public function __construct()
    {
        $this->botToken = config('services.telegram.bot_token');
        $this->chatId = config('services.telegram.chat_id');
    }

    /**                                                                                                                             
     * Kirim pesan notifikasi SMS ke Telegram                                                                                       
     */
    public function sendSmsNotification(Sms $sms): bool
    {
        if (empty($this->botToken) || empty($this->chatId)) {
            Log::warning('[Telegram] Bot Token atau Chat ID belum diisi di .env');
            return false;
        }

        // Format pesan rapi menggunakan HTML                                                                                       
        $deviceName = $sms->device ? $sms->device->name : 'Unknown Device';
        $receivedAt = $sms->received_at ? $sms->received_at->format('Y-m-d H:i:s') : now()->format('Y-m-d H:i:s');

        $text = " <b>SMS BARU DITERIMA</b>\n";
        $text .= "━━━━━━━━━━━━━━━━━━━━\n";
        $text .= " <b>Pengirim :</b> <code>" . htmlspecialchars($sms->phone) . "</code>\n";
        $text .= " <b>Perangkat :</b> " . htmlspecialchars($deviceName) . "\n";
        $text .= " <b>Waktu    :</b> " . htmlspecialchars($receivedAt) . "\n";
        $text .= "━━━━━━━━━━━━━━━━━━━━\n";
        $text .= " <b>Isi Pesan:</b>\n";
        $text .= htmlspecialchars($sms->message);

        return $this->sendMessage($text);
    }

    /**                                                                                                                             
     * Helper pengirim request HTTP ke API Telegram                                                                                 
     */
    public function sendMessage(string $message): bool
    {
        try {
            $url = "https://api.telegram.org/bot{$this->botToken}/sendMessage";

            $response = Http::timeout(5)->post($url, [
                'chat_id'    => $this->chatId,
                'text'       => $message,
                'parse_mode' => 'HTML',
            ]);

            if (!$response->successful()) {
                Log::error('[Telegram Error] ' . $response->body());
                return false;
            }

            return true;
        } catch (\Throwable $e) {
            Log::error('[Telegram Exception] Gagal kirim notifikasi: ' . $e->getMessage());
            return false;
        }
    }
}
