#pragma once

// The Modal Bowed exciter's bodies (ModalBowed.h), fitted by
// tools/fit_modal_bowed_body.py to ilana's Spitfire String Quartet renders (Long Normale, velocity 80, dry),
// then refined through the whole engine (swell, rosin, vibrato, wobble) by its Python twin:
// one grid of resonators (Hz and bandwidth for a violin; a viola's and a
// cello's are divided by their size), and each instrument's gains in dB.
// Partial error after the fit: violin 4.9 dB, viola 4.0 dB, cello 5.6 dB.
// Generated (tools/modal_bowed_refit/fit_body2.py): edit the tools, not this file.
struct ModalBowedBodies
{
    static constexpr int count = 36;
    static constexpr double viola = 1.17, cello = 2.10; // sizes against a violin
    static constexpr float hz[count] { 185.00f, 265.00f, 395.00f, 430.00f, 550.00f, 665.00f, 700.00f, 764.44f, 834.82f, 911.67f, 995.60f, 1087.25f, 1187.34f, 1296.65f, 1416.02f, 1546.38f, 1688.74f, 1844.20f, 2013.98f, 2199.38f, 2401.86f, 2622.97f, 2864.44f, 3128.14f, 3416.12f, 3730.60f, 4074.04f, 4449.09f, 4858.68f, 5305.96f, 5794.43f, 6327.86f, 6910.40f, 7546.57f, 8241.31f, 9000.00f };
    static constexpr float bandwidth[count] { 20.00f, 15.00f, 25.00f, 20.00f, 30.00f, 30.00f, 43.75f, 47.78f, 52.18f, 56.98f, 62.22f, 67.95f, 74.21f, 81.04f, 88.50f, 96.65f, 105.55f, 115.26f, 125.87f, 137.46f, 150.12f, 163.94f, 179.03f, 195.51f, 213.51f, 233.16f, 254.63f, 278.07f, 303.67f, 331.62f, 362.15f, 395.49f, 431.90f, 471.66f, 515.08f, 562.50f };
    static constexpr float violin[count] { -10.23f, -7.26f, 0.00f, -10.75f, -9.08f, -21.04f, -24.73f, -40.00f, -40.00f, -37.23f, -35.27f, -29.03f, -4.76f, -32.19f, -23.41f, -10.44f, -25.54f, -10.19f, -23.99f, -1.51f, -8.17f, -15.12f, -25.25f, -10.13f, -10.95f, -16.14f, -12.46f, -26.71f, -17.48f, -20.81f, -26.44f, -38.15f, -29.55f, -24.54f, -31.16f, -40.00f };
    static constexpr float violas[count] { -40.68f, -4.82f, -34.34f, -0.68f, -15.56f, -7.39f, -40.68f, -40.68f, -33.20f, -32.05f, -24.61f, -26.92f, -36.01f, -14.58f, -26.30f, -31.96f, -21.16f, -23.73f, -28.01f, -24.42f, -26.80f, -19.57f, -23.73f, -28.16f, -36.72f, -26.38f, -32.45f, -35.58f, -40.63f, -40.48f, -40.68f, -33.06f, -38.69f, -37.13f, -37.85f, -33.06f };
    static constexpr float cellos[count] { -48.78f, -23.37f, -22.71f, -19.14f, -27.39f, -27.67f, -25.56f, -22.48f, -25.61f, -36.55f, -35.08f, -22.99f, -20.38f, -21.78f, -24.27f, -23.85f, -18.13f, -27.16f, -25.16f, -25.48f, -23.90f, -27.98f, -24.21f, -24.61f, -19.22f, -20.94f, -15.60f, -14.52f, -14.20f, -12.70f, -17.66f, -17.74f, -18.22f, -16.79f, -23.96f, -15.66f };
};
