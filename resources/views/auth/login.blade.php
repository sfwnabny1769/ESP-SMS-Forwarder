<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Login Admin - SMS Gateway ESP32</title>

    <!-- Google Fonts: Inter -->
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">

    <!-- Bootstrap 5 CSS & Icons -->
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css">

    <!-- Custom Auth CSS -->
    <link rel="stylesheet" href="{{ asset('css/auth.css') }}">
</head>
<body>

    <div class="login-card">
        <!-- Logo / Icon -->
        <div class="brand-logo">
            <i class="bi bi-shield-lock-fill"></i>
        </div>

        <div class="text-center mb-4">
            <h4 class="fw-bold text-white mb-1">SMS Gateway Admin</h4>
            <p class="text-muted small">Silakan login untuk mengelola gateway & SMS</p>
        </div>

        <!-- Alert Notifikasi / Info -->
        @if (session('info'))
            <div class="alert alert-info py-2 px-3 small d-flex align-items-center mb-3" role="alert">
                <i class="bi bi-info-circle-fill me-2"></i>
                <div>{{ session('info') }}</div>
            </div>
        @endif

        @if (session('success'))
            <div class="alert alert-success py-2 px-3 small d-flex align-items-center mb-3" role="alert">
                <i class="bi bi-check-circle-fill me-2"></i>
                <div>{{ session('success') }}</div>
            </div>
        @endif

        @if ($errors->any())
            <div class="alert alert-danger py-2 px-3 small mb-3" role="alert">
                <div class="d-flex align-items-center mb-1">
                    <i class="bi bi-exclamation-triangle-fill me-2"></i>
                    <strong>Gagal Masuk:</strong>
                </div>
                <ul class="mb-0 ps-3">
                    @foreach ($errors->all() as $error)
                        <li>{{ $error }}</li>
                    @endforeach
                </ul>
            </div>
        @endif

        <!-- Form Login -->
        <form action="{{ route('login') }}" method="POST">
            @csrf

            <!-- Email Input -->
            <div class="mb-3">
                <label for="email" class="form-label">Alamat Email</label>
                <div class="input-group">
                    <span class="input-group-text"><i class="bi bi-envelope"></i></span>
                    <input type="email" 
                           class="form-control @error('email') is-invalid @enderror" 
                           id="email" 
                           name="email" 
                           value="{{ old('email') }}" 
                           placeholder="nama@domain.com" 
                           required 
                           autofocus 
                           autocomplete="email">
                </div>
            </div>

            <!-- Password Input with Toggle Visibility -->
            <div class="mb-3">
                <label for="password" class="form-label">Password</label>
                <div class="input-group">
                    <span class="input-group-text"><i class="bi bi-key"></i></span>
                    <input type="password" 
                           class="form-control @error('password') is-invalid @enderror" 
                           id="password" 
                           name="password" 
                           placeholder="••••••••" 
                           required 
                           autocomplete="current-password">
                    <button class="btn btn-outline-secondary" type="button" id="togglePassword" style="border-color: var(--border-color); color: #94a3b8;">
                        <i class="bi bi-eye" id="togglePasswordIcon"></i>
                    </button>
                </div>
            </div>

            <!-- Remember Me -->
            <div class="form-check mb-4">
                <input class="form-check-input" type="checkbox" name="remember" id="remember" {{ old('remember') ? 'checked' : '' }}>
                <label class="form-check-label" for="remember">
                    Ingat sesi saya di browser ini
                </label>
            </div>

            <!-- Submit Button -->
            <button type="submit" class="btn btn-primary w-100 mb-3">
                <i class="bi bi-box-arrow-in-right me-1"></i> Masuk ke Dashboard
            </button>
        </form>

        <div class="text-center mt-3">
            <small class="text-muted" style="font-size: 0.75rem;">
                <i class="bi bi-cpu me-1"></i> ESP32-S3 SIM800L Gateway Node
            </small>
        </div>
    </div>

    <!-- Script Toggle Password -->
    <script>
        const togglePassword = document.querySelector('#togglePassword');
        const passwordInput = document.querySelector('#password');
        const toggleIcon = document.querySelector('#togglePasswordIcon');

        togglePassword.addEventListener('click', function () {
            const type = passwordInput.getAttribute('type') === 'password' ? 'text' : 'password';
            passwordInput.setAttribute('type', type);
            toggleIcon.classList.toggle('bi-eye');
            toggleIcon.classList.toggle('bi-eye-slash');
        });
    </script>
</body>
</html>
