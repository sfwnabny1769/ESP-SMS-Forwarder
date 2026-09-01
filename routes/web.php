<?php

use App\Http\Controllers\Web\DashboardController;
use App\Http\Controllers\Web\DeviceController;
use App\Http\Controllers\Web\SmsController;
use App\Http\Controllers\Web\AuthController;
use Illuminate\Support\Facades\Route;

/*
|--------------------------------------------------------------------------
| Web Routes for SMS Gateway Dashboard
|--------------------------------------------------------------------------
*/

// Rute Guest
Route::middleware('guest')->group(function () {
    Route::get('/login', [AuthController::class, 'showLoginForm'])->name('login');
    Route::post('/login', [AuthController::class, 'login']);
});

// Rute Authenticated
Route::middleware('auth')->group(function () {
    //logout
    Route::post('/logout', [AuthController::class, 'logout'])->name('logout');
    //dashboard
    Route::get('/', DashboardController::class)->name('dashboard');
    Route::get('/dashboard', DashboardController::class);
    // Device Management Routes
    Route::resource('devices', DeviceController::class)->except(['create', 'edit']);
    Route::post('devices/{device}/regenerate-token', [DeviceController::class, 'regenerateToken'])->name('devices.regenerate-token');
    Route::post('devices/{device}/send-command', [DeviceController::class, 'sendCommand'])->name('devices.send-command');
    Route::post('devices/{device}/sync-sms', [DeviceController::class, 'syncSimSms'])->name('devices.sync-sms');
    Route::get('devices/{device}/command-status', [DeviceController::class, 'getCommandStatus'])->name('devices.command-status');
    // SMS Routes
    Route::resource('sms', SmsController::class)->only(['index', 'show', 'destroy']);
    Route::post('sms/sync-sim', [SmsController::class, 'syncFromSim'])->name('sms.sync-sim');
    Route::patch('sms/{sm}/toggle-processed', [SmsController::class, 'toggleProcessed'])->name('sms.toggle-processed');
});