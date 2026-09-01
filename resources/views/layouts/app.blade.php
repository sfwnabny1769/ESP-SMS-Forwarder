<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>@yield('title', 'Dashboard') - SMS Gateway ESP32</title>

    <!-- Google Fonts: Inter -->
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">

    <!-- Bootstrap 5 CSS -->
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">
    <!-- Bootstrap Icons -->
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css">
    <!-- Chart.js -->
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

    <style>
        :root {
            --primary-bg: #0f172a;
            --sidebar-bg: #1e293b;
            --sidebar-hover: #334155;
            --accent-blue: #3b82f6;
            --accent-green: #10b981;
            --card-border: #e2e8f0;
        }

        body {
            font-family: 'Inter', sans-serif;
            background-color: #f8fafc;
            color: #1e293b;
        }

        /* Sidebar Styling */
        #sidebar-wrapper {
            min-height: 100vh;
            width: 260px;
            background: var(--sidebar-bg);
            transition: margin 0.25s ease-out;
            position: fixed;
            top: 0;
            left: 0;
            z-index: 1000;
        }

        #sidebar-wrapper .sidebar-heading {
            padding: 1.25rem 1.5rem;
            font-size: 1.2rem;
            font-weight: 700;
            color: #ffffff;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
        }

        #sidebar-wrapper .list-group-item {
            padding: 0.85rem 1.5rem;
            color: #94a3b8;
            background: transparent;
            border: none;
            font-weight: 500;
            border-radius: 8px;
            margin: 0.2rem 0.8rem;
            display: flex;
            align-items: center;
            gap: 12px;
            transition: all 0.2s ease;
        }

        #sidebar-wrapper .list-group-item:hover,
        #sidebar-wrapper .list-group-item.active {
            color: #ffffff;
            background-color: var(--sidebar-hover);
        }

        #sidebar-wrapper .list-group-item.active {
            background-color: var(--accent-blue);
            font-weight: 600;
        }

        /* Main Content Wrapper */
        #page-content-wrapper {
            margin-left: 260px;
            width: calc(100% - 260px);
            min-height: 100vh;
        }

        .navbar-custom {
            background: #ffffff;
            border-bottom: 1px solid #e2e8f0;
            padding: 0.8rem 1.5rem;
        }

        /* Custom Cards */
        .stat-card {
            border: 1px solid var(--card-border);
            border-radius: 12px;
            background: #ffffff;
            transition: transform 0.2s ease, box-shadow 0.2s ease;
        }

        .stat-card:hover {
            transform: translateY(-3px);
            box-shadow: 0 10px 20px rgba(0, 0, 0, 0.05);
        }

        .stat-icon {
            width: 48px;
            height: 48px;
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.4rem;
        }

        .badge-online {
            background-color: #d1fae5;
            color: #065f46;
            font-weight: 600;
        }

        .badge-offline {
            background-color: #fee2e2;
            color: #991b1b;
            font-weight: 600;
        }

        .table-custom {
            background: #ffffff;
            border-radius: 12px;
            overflow: hidden;
            border: 1px solid var(--card-border);
        }

        .table-custom th {
            background-color: #f1f5f9;
            color: #475569;
            font-weight: 600;
            font-size: 0.85rem;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            border-bottom: 1px solid #e2e8f0;
        }

        @media (max-width: 992px) {
            #sidebar-wrapper {
                margin-left: -260px;
            }
            #sidebar-wrapper.toggled {
                margin-left: 0;
            }
            #page-content-wrapper {
                margin-left: 0;
                width: 100%;
            }
        }
    </style>
    @stack('styles')
</head>
<body>

<div class="d-flex" id="wrapper">

    <!-- Sidebar -->
    <div id="sidebar-wrapper">
        <div class="sidebar-heading d-flex align-items-center gap-2">
            <i class="bi bi-cpu-fill text-primary"></i>
            <span>SMS Gateway</span>
        </div>
        <div class="list-group list-group-flush mt-3">
            <a href="{{ route('dashboard') }}" class="list-group-item {{ request()->routeIs('dashboard') ? 'active' : '' }}">
                <i class="bi bi-grid-1x2-fill"></i> Dashboard
            </a>
            <a href="{{ route('sms.index') }}" class="list-group-item {{ request()->routeIs('sms.*') ? 'active' : '' }}">
                <i class="bi bi-chat-left-text-fill"></i> Data SMS
            </a>
            <a href="{{ route('devices.index') }}" class="list-group-item {{ request()->routeIs('devices.*') ? 'active' : '' }}">
                <i class="bi bi-router-fill"></i> Devices (ESP32)
            </a>
        </div>
    </div>

    <!-- Page Content -->
    <div id="page-content-wrapper">

        <!-- Top Navbar -->
        <nav class="navbar navbar-custom d-flex justify-content-between align-items-center">
            <div class="d-flex align-items-center gap-3">
                <button class="btn btn-outline-secondary d-lg-none" id="menu-toggle">
                    <i class="bi bi-list"></i>
                </button>
                <h5 class="m-0 fw-bold">@yield('page-title', 'Overview')</h5>
            </div>
            <div class="d-flex align-items-center gap-3">
                <span class="badge bg-light text-dark border p-2 d-none d-md-inline-block">
                    <i class="bi bi-clock me-1 text-primary"></i> {{ now()->format('d M Y H:i') }}
                </span>

                <!-- User Dropdown & Logout -->
                <div class="dropdown">
                    <button class="btn btn-outline-light dropdown-toggle d-flex align-items-center gap-2 border text-dark py-1 px-2" type="button" data-bs-toggle="dropdown" aria-expanded="false">
                        <div class="bg-primary text-white rounded-circle d-flex align-items-center justify-content-center fw-bold" style="width: 32px; height: 32px; font-size: 0.85rem;">
                            {{ strtoupper(substr(Auth::user()->name ?? 'A', 0, 1)) }}
                        </div>
                        <span class="fw-medium small d-none d-sm-inline">{{ Auth::user()->name ?? 'Administrator' }}</span>
                    </button>
                    <ul class="dropdown-menu dropdown-menu-end shadow-sm border-0 mt-2" style="min-width: 200px;">
                        <li class="px-3 py-2 border-bottom">
                            <div class="fw-bold small text-dark">{{ Auth::user()->name ?? 'Admin' }}</div>
                            <div class="text-muted" style="font-size: 0.75rem;">{{ Auth::user()->email ?? '' }}</div>
                        </li>
                        <li>
                            <form action="{{ route('logout') }}" method="POST" class="m-0">
                                @csrf
                                <button type="submit" class="dropdown-item text-danger py-2 d-flex align-items-center gap-2">
                                    <i class="bi bi-box-arrow-right"></i> Keluar (Logout)
                                </button>
                            </form>
                        </li>
                    </ul>
                </div>
            </div>
        </nav>

        <!-- Main Body -->
        <div class="container-fluid p-4">

            <!-- Alerts -->
            @if(session('success'))
                <div class="alert alert-success alert-dismissible fade show border-0 shadow-sm" role="alert">
                    <i class="bi bi-check-circle-fill me-2"></i> {{ session('success') }}
                    <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
                </div>
            @endif

            @if(session('error'))
                <div class="alert alert-danger alert-dismissible fade show border-0 shadow-sm" role="alert">
                    <i class="bi bi-exclamation-triangle-fill me-2"></i> {{ session('error') }}
                    <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
                </div>
            @endif

            @if($errors->any())
                <div class="alert alert-danger alert-dismissible fade show border-0 shadow-sm" role="alert">
                    <i class="bi bi-x-circle-fill me-2"></i> <strong>Terjadi kesalahan input:</strong>
                    <ul class="mb-0 mt-1 ps-3">
                        @foreach($errors->all() as $error)
                            <li>{{ $error }}</li>
                        @endforeach
                    </ul>
                    <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
                </div>
            @endif

            @yield('content')

        </div>

    </div>

</div>

<!-- Bootstrap 5 JS Bundle -->
<script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>

<script>
    // Sidebar toggle for mobile view
    document.getElementById("menu-toggle")?.addEventListener("click", function(e) {
        e.preventDefault();
        document.getElementById("sidebar-wrapper").classList.toggle("toggled");
    });
</script>

@stack('scripts')
</body>
</html>
