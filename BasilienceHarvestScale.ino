#include "LoadCellManager.h"

// ============================================================
// BASILIENCE HARVEST SCALE
// Phase 2C - Refined Calibration
// ESP8266 + HX711 + 20 kg Load Cell
// ============================================================

// ------------------------------------------------------------
// PIN ASSIGNMENTS
// ------------------------------------------------------------

constexpr uint8_t HX711_DOUT_PIN = D5;   // GPIO14
constexpr uint8_t HX711_SCK_PIN  = D6;   // GPIO12

// ------------------------------------------------------------
// CALIBRATION
// ------------------------------------------------------------
//
// Previous factor:
// 128.17
//
// Controlled test:
// Known reference = 196 g
// Average measured value ≈ 190.6 g
//
// Refined calibration factor ≈ 124.64
//

constexpr float CALIBRATION_FACTOR = 124.64f;

// ------------------------------------------------------------
// SCALE SETTINGS
// ------------------------------------------------------------

constexpr uint8_t TARE_SAMPLES = 30;

constexpr uint8_t READING_SAMPLES = 10;

constexpr unsigned long HX711_TIMEOUT_MS = 1500;

constexpr unsigned long READING_INTERVAL_MS = 500;

// ------------------------------------------------------------
// WARM-UP
// ------------------------------------------------------------
//
// HX711 and load cell showed noticeable startup drift.
// Therefore the system is allowed to stabilize before tare.
//

constexpr unsigned long WARMUP_TIME_MS = 60000;

// ------------------------------------------------------------
// ZERO DEADBAND
// ------------------------------------------------------------
//
// Small zero drift around +/- 5 grams is treated as zero.
//

constexpr float ZERO_DEADBAND_GRAMS = 5.0f;

// ------------------------------------------------------------
// SCALE CAPACITY
// ------------------------------------------------------------

constexpr float MAX_WEIGHT_GRAMS = 20000.0f;

// ------------------------------------------------------------
// OBJECTS
// ------------------------------------------------------------

LoadCellManager loadCell(
    HX711_DOUT_PIN,
    HX711_SCK_PIN
);

unsigned long lastReadingTime = 0;

// ============================================================
// WARM-UP
// ============================================================

void warmUpScale()
{
    Serial.println();
    Serial.println("====================================");
    Serial.println(" SCALE WARM-UP");
    Serial.println("====================================");
    Serial.println();

    Serial.println("[SCALE] Keep platform EMPTY.");
    Serial.println("[SCALE] Do NOT touch the scale.");
    Serial.println();

    Serial.println(
        "[SCALE] Stabilizing load cell and HX711..."
    );

    unsigned long startTime = millis();
    unsigned long lastPrintTime = 0;

    while (
        millis() - startTime
        <
        WARMUP_TIME_MS
    )
    {
        yield();

        long rawValue = 0;

        if (
            loadCell.readRawAverage(
                5,
                rawValue,
                HX711_TIMEOUT_MS
            )
        )
        {
            if (
                millis() - lastPrintTime
                >=
                5000
            )
            {
                lastPrintTime = millis();

                unsigned long elapsed =
                    millis() - startTime;

                unsigned long remaining =
                    (
                        WARMUP_TIME_MS
                        -
                        elapsed
                    )
                    /
                    1000;

                Serial.print("[WARMUP] Raw: ");
                Serial.print(rawValue);

                Serial.print(" | Remaining: ");
                Serial.print(remaining);

                Serial.println(" sec");
            }
        }

        delay(10);
    }

    Serial.println();
    Serial.println("[SCALE] Warm-up complete.");
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("      Basilience Harvest Scale");
    Serial.println("====================================");
    Serial.println();

    // --------------------------------------------------------
    // INITIALIZE HX711
    // --------------------------------------------------------

    Serial.println("[SCALE] Initializing HX711...");

    if (
        !loadCell.begin(
            HX711_TIMEOUT_MS
        )
    )
    {
        Serial.println();
        Serial.println(
            "[SCALE] ERROR: HX711 not detected."
        );

        Serial.println();
        Serial.println("Check wiring:");
        Serial.println("VCC -> ESP8266 3V3");
        Serial.println("GND -> ESP8266 GND");
        Serial.println("DT  -> ESP8266 D5");
        Serial.println("SCK -> ESP8266 D6");

        return;
    }

    Serial.println("[SCALE] HX711 detected.");

    // --------------------------------------------------------
    // CALIBRATION
    // --------------------------------------------------------

    loadCell.setCalibrationFactor(
        CALIBRATION_FACTOR
    );

    Serial.print(
        "[SCALE] Calibration factor: "
    );

    Serial.println(
        CALIBRATION_FACTOR,
        2
    );

    // --------------------------------------------------------
    // WARM-UP
    // --------------------------------------------------------

    warmUpScale();

    // --------------------------------------------------------
    // TARE
    // --------------------------------------------------------

    Serial.println();
    Serial.println("====================================");
    Serial.println(" TARE");
    Serial.println("====================================");
    Serial.println();

    Serial.println(
        "[SCALE] Platform must be EMPTY."
    );

    Serial.println(
        "[SCALE] Taring..."
    );

    if (
        !loadCell.tare(
            TARE_SAMPLES,
            HX711_TIMEOUT_MS
        )
    )
    {
        Serial.println(
            "[SCALE] ERROR: Unable to tare."
        );

        return;
    }

    Serial.println(
        "[SCALE] Tare complete."
    );

    Serial.print(
        "[SCALE] Tare offset: "
    );

    Serial.println(
        loadCell.getOffset()
    );

    Serial.println();
    Serial.println("====================================");
    Serial.println(" SCALE READY");
    Serial.println("====================================");
    Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // READING INTERVAL
    // --------------------------------------------------------

    if (
        millis() - lastReadingTime
        <
        READING_INTERVAL_MS
    )
    {
        return;
    }

    lastReadingTime = millis();

    // --------------------------------------------------------
    // READ WEIGHT
    // --------------------------------------------------------

    float weightGrams = 0.0f;

    bool success =
        loadCell.readWeightGrams(
            READING_SAMPLES,
            weightGrams,
            HX711_TIMEOUT_MS
        );

    if (!success)
    {
        Serial.println(
            "[SCALE] ERROR: HX711 read timeout."
        );

        return;
    }

    // --------------------------------------------------------
    // ZERO DEADBAND
    // --------------------------------------------------------

    if (
        weightGrams >= -ZERO_DEADBAND_GRAMS
        &&
        weightGrams <= ZERO_DEADBAND_GRAMS
    )
    {
        weightGrams = 0.0f;
    }

    // --------------------------------------------------------
    // NEGATIVE WEIGHT HANDLING
    // --------------------------------------------------------
    //
    // A tiny negative value is just zero drift.
    // Don't display negative harvest weights.
    //

    if (
        weightGrams < 0.0f
        &&
        weightGrams > -20.0f
    )
    {
        weightGrams = 0.0f;
    }

    // --------------------------------------------------------
    // SANITY VALIDATION
    // --------------------------------------------------------

    if (
        weightGrams >
        MAX_WEIGHT_GRAMS
    )
    {
        Serial.println(
            "[SCALE] ERROR: Weight exceeds 20 kg."
        );

        return;
    }

    if (
        weightGrams <
        -1000.0f
    )
    {
        Serial.println(
            "[SCALE] ERROR: Invalid negative reading."
        );

        return;
    }

    // --------------------------------------------------------
    // CONVERT TO KG
    // --------------------------------------------------------

    float weightKg =
        weightGrams / 1000.0f;

    // --------------------------------------------------------
    // OUTPUT
    // --------------------------------------------------------

    Serial.print("[SCALE] ");

    Serial.print(
        weightGrams,
        1
    );

    Serial.print(" g");

    Serial.print(" | ");

    Serial.print(
        weightKg,
        3
    );

    Serial.println(" kg");
}