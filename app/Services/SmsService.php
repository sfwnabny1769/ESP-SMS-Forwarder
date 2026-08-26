<?php

namespace App\Services;

use App\Models\Device;
use App\Models\Sms;
use Illuminate\Contracts\Pagination\LengthAwarePaginator;
use Illuminate\Support\Carbon;
use Illuminate\Support\Facades\DB;

class SmsService
{
    //nganuin telegramservoce biar masuk sini
    public function __construct(
        protected TelegramService $TelegramService
        ){
    }
    /**
     * Store incoming SMS forwarded by ESP32 gateway.
     */
    public function storeIncomingSms(Device $device, array $data): Sms
    {
        $receivedAt = !empty($data['received_at'])
            ? Carbon::parse($data['received_at'])
            : now();

// simpan ke database

        $sms = Sms::create([
            'device_id' => $device->id,
            'phone' => $data['phone'],
            'message' => $data['message'],
            'received_at' => $receivedAt,
            'processed' => $data['processed'] ?? false,
        ]);

        // kirim ke telegram
        $this->TelegramService->sendSmsNotification($sms);

        return $sms;
    }



    /**
     * Get paginated SMS logs with search & device filtering.
     */
    public function getPaginatedSms(array $filters = [], int $perPage = 15): LengthAwarePaginator
    {
        $search = $filters['search'] ?? null;
        $deviceId = $filters['device_id'] ?? null;
        $processed = $filters['processed'] ?? null;

        return Sms::with('device')
            ->search($search)
            ->filterByDevice($deviceId)
            ->when(!is_null($processed), function ($q) use ($processed) {
                $q->where('processed', (bool) $processed);
            })
            ->latest('received_at')
            ->latest('id')
            ->paginate($perPage)
            ->withQueryString();
    }

    /**
     * Get statistics for dashboard overview.
     */
    public function getSmsStats(): array
    {
        $todayDate = now()->toDateString();

        $totalSms = Sms::count();

        $todaySms = Sms::whereDate('received_at', $todayDate)
            ->orWhere(function ($query) use ($todayDate) {
                $query->whereNull('received_at')
                      ->whereDate('created_at', $todayDate);
            })
            ->count();

        $unprocessedCount = Sms::where('processed', false)->count();

        return [
            'total_sms' => $totalSms,
            'sms_today' => $todaySms,
            'unprocessed_sms' => $unprocessedCount,
        ];
    }

    /**
     * Get SMS count per day for chart visualization (default past 7 days).
     */
    public function getDailySmsChartData(int $days = 7): array
    {
        $labels = [];
        $data = [];

        for ($i = $days - 1; $i >= 0; $i--) {
            $date = now()->subDays($i);
            $dateStr = $date->toDateString();

            $count = Sms::whereDate('received_at', $dateStr)
                ->orWhere(function ($query) use ($dateStr) {
                    $query->whereNull('received_at')
                          ->whereDate('created_at', $dateStr);
                })
                ->count();

            $labels[] = $date->format('d M');
            $data[] = $count;
        }

        return [
            'labels' => $labels,
            'data' => $data,
        ];
    }

    /**
     * Delete SMS record.
     */
    public function deleteSms(Sms $sms): bool
    {
        return (bool) $sms->delete();
    }

    /**
     * Toggle processed state of SMS.
     */
    public function toggleProcessed(Sms $sms): Sms
    {
        $sms->update(['processed' => !$sms->processed]);
        return $sms->fresh();
    }
}
