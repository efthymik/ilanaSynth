#pragma once

// The Modal Bowed exciter's bodies (ModalBowed.h), fitted by
// tools/fit_modal_bowed_body.py to ilana's Spitfire String Quartet renders (Long Normale, velocity 80, dry),
// then refined through the whole engine (swell, rosin, vibrato) by its Python twin:
// one grid of resonators (Hz and bandwidth for a violin; a viola's and a
// cello's are divided by their size), and each instrument's gains in dB.
// Partial error after the fit: violin 3.9 dB, viola 4.4 dB, cello 6.1 dB.
// Generated (tools/modal_bowed_refit/fit_body2.py): edit the tools, not this file.
struct ModalBowedBodies
{
    static constexpr int count = 36;
    static constexpr double viola = 1.17, cello = 2.10; // sizes against a violin
    static constexpr float hz[count] { 185.00f, 265.00f, 395.00f, 430.00f, 550.00f, 665.00f, 700.00f, 764.44f, 834.82f, 911.67f, 995.60f, 1087.25f, 1187.34f, 1296.65f, 1416.02f, 1546.38f, 1688.74f, 1844.20f, 2013.98f, 2199.38f, 2401.86f, 2622.97f, 2864.44f, 3128.14f, 3416.12f, 3730.60f, 4074.04f, 4449.09f, 4858.68f, 5305.96f, 5794.43f, 6327.86f, 6910.40f, 7546.57f, 8241.31f, 9000.00f };
    static constexpr float bandwidth[count] { 20.00f, 15.00f, 25.00f, 20.00f, 30.00f, 30.00f, 43.75f, 47.78f, 52.18f, 56.98f, 62.22f, 67.95f, 74.21f, 81.04f, 88.50f, 96.65f, 105.55f, 115.26f, 125.87f, 137.46f, 150.12f, 163.94f, 179.03f, 195.51f, 213.51f, 233.16f, 254.63f, 278.07f, 303.67f, 331.62f, 362.15f, 395.49f, 431.90f, 471.66f, 515.08f, 562.50f };
    static constexpr float violin[count] { -8.29f, -7.47f, 0.00f, -29.01f, -14.36f, -18.20f, -22.78f, -23.31f, -27.02f, -23.54f, -26.67f, -21.52f, -13.07f, -24.67f, -22.50f, -13.98f, -18.39f, -16.09f, -20.06f, -11.70f, -11.44f, -15.65f, -19.52f, -14.10f, -12.29f, -18.30f, -19.54f, -26.75f, -26.69f, -23.04f, -20.88f, -26.96f, -25.25f, -23.11f, -24.09f, -40.00f };
    static constexpr float violas[count] { -29.58f, -6.06f, -29.12f, -0.68f, -17.09f, -9.47f, -40.68f, -28.02f, -27.10f, -30.35f, -24.75f, -28.50f, -32.61f, -14.27f, -20.11f, -27.18f, -17.62f, -19.14f, -23.57f, -19.71f, -21.84f, -13.64f, -13.51f, -16.28f, -21.25f, -14.97f, -17.11f, -22.08f, -33.97f, -34.23f, -35.49f, -25.24f, -29.88f, -34.47f, -37.33f, -29.69f };
    static constexpr float cellos[count] { -34.29f, -25.40f, -34.26f, -25.42f, -26.89f, -28.31f, -30.60f, -27.10f, -26.67f, -30.19f, -29.70f, -29.90f, -28.05f, -29.15f, -29.09f, -28.17f, -28.59f, -31.06f, -30.00f, -31.73f, -29.64f, -31.66f, -31.68f, -30.64f, -29.38f, -29.02f, -27.17f, -27.37f, -27.55f, -28.33f, -29.04f, -29.21f, -29.80f, -30.12f, -32.97f, -26.63f };
};
