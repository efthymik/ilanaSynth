#pragma once

// The Modal Bowed exciter's bodies (ModalBowed.h), fitted by
// tools/fit_modal_bowed_body.py to the University of Iowa 2012 violin, viola
// and cello (arco, mf): one grid of resonators (Hz and bandwidth for a
// violin; a viola's and a cello's are divided by their size), and each
// instrument's gains in dB. Partial error after the fit: violin 6.1 dB,
// viola 8.5 dB, cello 7.3 dB. Generated: edit the tool, not this file.
struct ModalBowedBodies
{
    static constexpr int count = 36;
    static constexpr double viola = 1.17, cello = 2.10; // sizes against a violin
    static constexpr float hz[count] { 185.00f, 265.00f, 395.00f, 430.00f, 550.00f, 665.00f, 700.00f, 764.44f, 834.82f, 911.67f, 995.60f, 1087.25f, 1187.34f, 1296.65f, 1416.02f, 1546.38f, 1688.74f, 1844.20f, 2013.98f, 2199.38f, 2401.86f, 2622.97f, 2864.44f, 3128.14f, 3416.12f, 3730.60f, 4074.04f, 4449.09f, 4858.68f, 5305.96f, 5794.43f, 6327.86f, 6910.40f, 7546.57f, 8241.31f, 9000.00f };
    static constexpr float bandwidth[count] { 20.00f, 15.00f, 25.00f, 20.00f, 30.00f, 30.00f, 43.75f, 47.78f, 52.18f, 56.98f, 62.22f, 67.95f, 74.21f, 81.04f, 88.50f, 96.65f, 105.55f, 115.26f, 125.87f, 137.46f, 150.12f, 163.94f, 179.03f, 195.51f, 213.51f, 233.16f, 254.63f, 278.07f, 303.67f, 331.62f, 362.15f, 395.49f, 431.90f, 471.66f, 515.08f, 562.50f };
    static constexpr float violin[count] { -21.94f, 0.00f, -27.10f, -0.89f, -3.65f, -35.60f, -17.69f, -15.16f, -12.40f, -12.94f, -12.61f, -17.87f, -29.54f, -17.93f, -33.32f, -40.00f, -5.82f, -25.56f, -7.59f, -6.79f, -12.72f, -11.38f, -9.37f, -7.71f, -4.94f, -2.64f, -4.45f, -2.65f, -27.31f, -37.61f, -28.14f, -28.01f, -40.00f, -28.00f, -34.48f, -40.00f };
    static constexpr float violas[count] { -23.71f, -23.97f, -31.34f, -9.75f, -32.07f, -18.23f, -18.72f, -16.01f, -15.85f, -9.54f, -16.63f, 0.53f, -23.01f, -3.19f, -16.84f, -12.44f, -9.90f, -7.38f, -10.79f, -12.66f, -12.73f, -12.70f, -8.93f, -7.34f, -10.43f, -12.05f, -11.35f, -11.73f, -11.96f, -13.24f, -14.63f, -11.66f, -14.02f, -13.56f, -13.18f, -9.84f };
    static constexpr float cellos[count] { -23.33f, -15.69f, -6.23f, -23.41f, -13.65f, -21.30f, -26.90f, -18.16f, -27.82f, -39.92f, -17.24f, -32.04f, -22.79f, -24.10f, -24.57f, -28.75f, -22.18f, -37.53f, -24.90f, -24.27f, -27.26f, -26.85f, -26.45f, -36.17f, -20.82f, -33.91f, -31.12f, -22.95f, -24.79f, -17.45f, -37.49f, -33.76f, -34.76f, -36.08f, -35.88f, -24.96f };
};
