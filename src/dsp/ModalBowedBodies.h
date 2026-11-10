#pragma once

// The Modal Bowed exciter's bodies (ModalBowed.h), fitted by
// tools/fit_modal_bowed_body.py to ilana's Spitfire String Quartet renders (Long Normale, velocity 80, dry),
// then refined through the whole engine (swell, rosin, vibrato, wobble) by its Python twin:
// one grid of resonators (Hz and bandwidth for a violin; a viola's and a
// cello's are divided by their size), and each instrument's gains in dB.
// Partial error after the fit: violin 4.6 dB, viola 4.0 dB, cello 3.6 dB.
// Generated (tools/modal_bowed_refit/fit_body2.py): edit the tools, not this file.
struct ModalBowedBodies
{
    static constexpr int count = 36;
    static constexpr double viola = 1.17, cello = 2.10; // sizes against a violin
    static constexpr float hz[count] { 185.00f, 265.00f, 395.00f, 430.00f, 550.00f, 665.00f, 700.00f, 764.44f, 834.82f, 911.67f, 995.60f, 1087.25f, 1187.34f, 1296.65f, 1416.02f, 1546.38f, 1688.74f, 1844.20f, 2013.98f, 2199.38f, 2401.86f, 2622.97f, 2864.44f, 3128.14f, 3416.12f, 3730.60f, 4074.04f, 4449.09f, 4858.68f, 5305.96f, 5794.43f, 6327.86f, 6910.40f, 7546.57f, 8241.31f, 9000.00f };
    static constexpr float bandwidth[count] { 20.00f, 15.00f, 25.00f, 20.00f, 30.00f, 30.00f, 43.75f, 47.78f, 52.18f, 56.98f, 62.22f, 67.95f, 74.21f, 81.04f, 88.50f, 96.65f, 105.55f, 115.26f, 125.87f, 137.46f, 150.12f, 163.94f, 179.03f, 195.51f, 213.51f, 233.16f, 254.63f, 278.07f, 303.67f, 331.62f, 362.15f, 395.49f, 431.90f, 471.66f, 515.08f, 562.50f };
    static constexpr float violin[count] { -40.00f, 0.00f, -2.48f, -5.95f, -8.04f, -26.80f, -31.34f, -35.56f, -33.44f, -28.51f, -27.80f, -30.71f, -18.10f, -40.00f, -32.89f, -16.36f, -22.61f, -10.00f, -28.26f, -8.05f, -11.38f, -9.27f, -14.77f, -2.39f, -5.70f, -13.08f, -11.29f, -22.24f, -11.43f, -11.99f, -16.33f, -31.47f, -32.54f, -25.86f, -33.55f, -40.00f };
    static constexpr float violas[count] { -40.68f, -4.82f, -34.34f, -0.68f, -15.56f, -7.39f, -40.68f, -40.68f, -33.20f, -32.05f, -24.61f, -26.92f, -36.01f, -14.58f, -26.30f, -31.96f, -21.16f, -23.73f, -28.01f, -24.42f, -26.80f, -19.57f, -23.73f, -28.16f, -36.72f, -26.38f, -32.45f, -35.58f, -40.63f, -40.48f, -40.68f, -33.06f, -38.69f, -37.13f, -37.85f, -33.06f };
    static constexpr float cellos[count] { -48.01f, -48.70f, -22.53f, -16.95f, -24.79f, -32.37f, -27.19f, -17.26f, -18.81f, -48.70f, -48.70f, -45.30f, -15.91f, -13.61f, -24.79f, -19.91f, -9.77f, -22.07f, -24.71f, -21.77f, -22.65f, -26.99f, -19.38f, -25.83f, -12.16f, -15.33f, -10.59f, -8.70f, -11.47f, -8.92f, -17.95f, -21.00f, -20.89f, -13.84f, -31.94f, -27.92f };
};
