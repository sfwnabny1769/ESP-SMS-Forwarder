<?php

use App\Http\Controllers\Api\DeviceController;
use App\Http\Controllers\Api\SmsController;
use Illuminate\Support\Facades\Route;

/*
|--------------------------------------------------------------------------
| REST API Routes for ESP32 + SIM800L Gateway
|--------------------------------------------------------------------------
*/

Route::prefix('device')->group(function () {
    Route::post('/heartbeat', [DeviceController::class, 'heartbeat'])->middleware('device.token');
    Route::post('/command-response', [DeviceController::class, 'commandResponse'])->middleware('device.token');
});

Route::prefix('sms')->group(function () {
    Route::post('/', [SmsController::class, 'store'])->middleware('device.token');
});
