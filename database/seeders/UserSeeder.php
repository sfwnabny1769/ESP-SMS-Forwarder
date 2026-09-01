<?php

namespace Database\Seeders;

use Illuminate\Database\Console\Seeds\WithoutModelEvents;
use Illuminate\Database\Seeder;
use App\Models\User;

class UserSeeder extends Seeder
{
    /**
     * Run the database seeds.
     */
    public function run(): void
    {
        User::firstOrCreate(
            ['email' => 'abin.shafwan@gmail.com'],
            [
                'name' => 'Admin',
                'password' => bcrypt('Teti1769'),
            ]
        );
    }
}
