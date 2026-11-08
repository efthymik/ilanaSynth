#pragma once

// The Modal Bowed exciter's bodies (ModalBowed.h), fitted by
// tools/fit_modal_bowed_body.py to ilana's Spitfire String Quartet renders (Long Normale, velocity 80, dry):
// one grid of resonators (Hz and bandwidth for a violin; a viola's and a
// cello's are divided by their size), and each instrument's gains in dB.
// Partial error after the fit: violin 4.8 dB, viola 4.5 dB, cello 5.8 dB.
// Generated: edit the tool, not this file.
struct ModalBowedBodies
{
    static constexpr int count = 36;
    static constexpr double viola = 1.17, cello = 2.10; // sizes against a violin
    static constexpr float hz[count] { 185.00f, 265.00f, 395.00f, 430.00f, 550.00f, 665.00f, 700.00f, 764.44f, 834.82f, 911.67f, 995.60f, 1087.25f, 1187.34f, 1296.65f, 1416.02f, 1546.38f, 1688.74f, 1844.20f, 2013.98f, 2199.38f, 2401.86f, 2622.97f, 2864.44f, 3128.14f, 3416.12f, 3730.60f, 4074.04f, 4449.09f, 4858.68f, 5305.96f, 5794.43f, 6327.86f, 6910.40f, 7546.57f, 8241.31f, 9000.00f };
    static constexpr float bandwidth[count] { 20.00f, 15.00f, 25.00f, 20.00f, 30.00f, 30.00f, 43.75f, 47.78f, 52.18f, 56.98f, 62.22f, 67.95f, 74.21f, 81.04f, 88.50f, 96.65f, 105.55f, 115.26f, 125.87f, 137.46f, 150.12f, 163.94f, 179.03f, 195.51f, 213.51f, 233.16f, 254.63f, 278.07f, 303.67f, 331.62f, 362.15f, 395.49f, 431.90f, 471.66f, 515.08f, 562.50f };
    static constexpr float violin[count] { -4.39f, -1.37f, 0.00f, -11.04f, -17.48f, -21.94f, -18.97f, -18.83f, -24.72f, -13.58f, -22.04f, -16.22f, -13.37f, -22.47f, -20.27f, -14.48f, -19.63f, -17.21f, -17.08f, -12.71f, -12.33f, -13.12f, -17.92f, -15.19f, -15.54f, -16.79f, -18.44f, -20.53f, -27.75f, -27.24f, -24.06f, -27.21f, -28.92f, -25.82f, -26.19f, -38.54f };
    static constexpr float violas[count] { -25.08f, -13.30f, -19.91f, -0.68f, -19.03f, -22.99f, -32.18f, -22.50f, -25.63f, -31.31f, -20.13f, -31.60f, -27.21f, -20.65f, -20.27f, -28.03f, -20.89f, -23.26f, -27.78f, -19.65f, -23.83f, -19.78f, -17.99f, -21.20f, -25.94f, -19.38f, -23.60f, -25.87f, -33.81f, -35.24f, -37.93f, -32.14f, -33.66f, -36.93f, -40.68f, -34.93f };
    static constexpr float cellos[count] { -21.59f, -12.70f, -21.56f, -12.72f, -14.19f, -15.61f, -17.90f, -14.40f, -13.97f, -17.49f, -17.00f, -17.20f, -15.35f, -16.45f, -16.39f, -15.47f, -15.89f, -18.36f, -17.30f, -19.03f, -16.94f, -18.96f, -18.98f, -17.94f, -16.68f, -16.32f, -14.47f, -14.67f, -14.85f, -15.63f, -16.34f, -16.51f, -17.10f, -17.42f, -20.27f, -13.93f };
};
