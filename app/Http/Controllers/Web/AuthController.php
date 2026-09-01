<?php

namespace App\Http\Controllers\Web;

use App\Http\Controllers\Controller;                                                                                                
use Illuminate\Http\RedirectResponse;                                                                                               
use Illuminate\Http\Request;                                                                                                        
use Illuminate\Support\Facades\Auth;                                                                                                
use Illuminate\Support\Facades\RateLimiter;                                                                                         
use Illuminate\Support\Str;                                                                                                         
use Illuminate\Validation\ValidationException;                                                                                      
use Illuminate\View\View;

class AuthController extends Controller
{
    //Tampilkan laman form login//
    public function showLoginForm(): View|RedirectResponse
    {
        if (Auth::check()) {
            return redirect()->route('dashboard');
        }

        return view('auth.login');
    }
    //autentikasi login dengan proteksi anti brute force dan session fixation//
    public function login(Request $request): RedirectResponse
    {
        //validasi format input
        $credentials = $request->validate([
            'email' => ['required','string','email'],
            'password' => ['required','string'],
        ], [
            'email.required' => 'Email wajib diisi.',
            'email.email' => 'Format email tidak valid.',
            'password.required' => 'Password wajib diisi.',
        ]);
        //remember me
        $remember = $request->boolean('remember');

        //kunci throttle untuk proteksi brute force (email + IP)
        $throttleKey = Str::transliterate(Str::lower($request->input('email')).'|'.$request->ip());

        //cek apakah ip/email sedang diblokir karena terlalu banyak percobaan login gagal
        if (RateLimiter::tooManyAttempts($throttleKey, 5)) {
            $seconds = RateLimiter::availableIn($throttleKey);
            throw ValidationException::withMessages([
                'email' => "Terlalu banyak percobaan login. Silakan coba lagi dalam $seconds detik.",
            ]);
        }
        
        //verifikasi kredensial login ke db
        if (Auth::attempt($credentials, $remember)) {
            //reset percobaan login gagal jika berhasil
            RateLimiter::clear($throttleKey);

            //regenerasi session untuk proteksi session fixation
            $request->session()->regenerate();

            return redirect()->intended(route('dashboard'))
                ->with('success', 'Login berhasil. Selamat datang, '.Auth::user()->name.'.');

        }

        //jika gagal, catat percobaan login gagal
        RateLimiter::hit($throttleKey, 60);

        throw ValidationException::withMessages([
            'email' => 'Email atau password salah.',
        ]);
    }

    //logout user dan hapus session//
    public function logout(Request $request): RedirectResponse
    {
        Auth::logout();
        //hapus session dan regenerate csrf
        $request->session()->invalidate();
        $request->session()->regenerateToken();

        return redirect()->route('login')
            ->with('success', 'Anda telah berhasil logout.');
    }
}
