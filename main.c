/*
 * AstroMedic AI - prototype triage assistant for astronauts (C, console)
 * Rules decide escalation. Steps use ONLY items in the on-board kit.
 * Prototype for demonstration. Not a substitute for flight surgeon advice.
 *
 * Build:  gcc main.c -o astromedic
 * Run:    ./astromedic
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

typedef enum { SELF_TREAT, MONITOR, CONTACT_GROUND, EMERGENCY } Level;
static const char *LEVEL_NAME[] = { "SELF-TREAT", "MONITOR", "CONTACT GROUND (flight surgeon)", "EMERGENCY" };

/* ---------- On-board kit (edit quantities to simulate shortages) ---------- */
typedef struct { const char *name; int qty; } Item;
static Item kit[] = {
    {"anti-nausea tablets", 10}, {"painkillers", 6}, {"antibiotics", 8},
    {"IV fluids", 4}, {"eye drops", 5}, {"thermometer", 1},
    {"ultrasound", 1}, {"blood pressure cuff", 1}, {"bandages", 20}
};
#define KIT_N (int)(sizeof(kit) / sizeof(kit[0]))

static int has(const char *name) {
    for (int i = 0; i < KIT_N; i++)
        if (strcmp(kit[i].name, name) == 0) return kit[i].qty > 0;
    return 0;
}
static void show_kit(void) {
    printf("\nON-BOARD KIT\n");
    for (int i = 0; i < KIT_N; i++) printf("  - %-22s x%d\n", kit[i].name, kit[i].qty);
}

/* ---------- Result buffer ---------- */
typedef struct { Level lvl; char condition[48]; char reason[512]; char steps[1536]; } Result;

static void add(char *buf, size_t size, const char *fmt, ...) {
    size_t len = strlen(buf);
    if (len >= size - 1) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf + len, size - len, fmt, ap);
    va_end(ap);
}
#define STEP(r, ...) add((r)->steps, sizeof((r)->steps), __VA_ARGS__)
#define WHY(r, ...)  add((r)->reason, sizeof((r)->reason), __VA_ARGS__)
static void raise_to(Result *r, Level l) { if (l > r->lvl) r->lvl = l; }

/* ---------- Input helpers ---------- */
static int ask_yn(const char *q) {
    char line[64];
    for (;;) {
        printf("%s (y/n): ", q);
        if (!fgets(line, sizeof line, stdin)) return 0;
        if (line[0] == 'y' || line[0] == 'Y') return 1;
        if (line[0] == 'n' || line[0] == 'N') return 0;
    }
}
static int ask_int(const char *q, int lo, int hi) {
    char line[64]; int v;
    for (;;) {
        printf("%s (%d-%d): ", q, lo, hi);
        if (!fgets(line, sizeof line, stdin)) return lo;
        if (sscanf(line, "%d", &v) == 1 && v >= lo && v <= hi) return v;
    }
}

/* ---------- Global red flags (checked first for every complaint) ---------- */
static int red_flags(Result *r) {
    printf("\nSAFETY CHECK\n");
    int chest = ask_yn("Chest pain or pressure?");
    int breath = ask_yn("Difficulty breathing?");
    int faint = ask_yn("Fainting or near-fainting?");
    int harm = ask_yn("Thoughts of harming yourself?");
    if (chest || breath || faint || harm) {
        r->lvl = EMERGENCY;
        strcpy(r->condition, "Red-flag symptom");
        WHY(r, "A red-flag symptom was reported (chest pain, breathing trouble, fainting or self-harm thoughts). ");
        STEP(r, "1. Alert the crew now and stay with the patient.\n");
        STEP(r, "2. Open an emergency link to the ground flight surgeon.\n");
        if (harm) STEP(r, "3. Do not leave the person alone. Speak to a crew member or ground now.\n");
        else      STEP(r, "3. Keep the patient calm and secured. Monitor breathing and pulse.\n");
        return 1;
    }
    return 0;
}

/* ---------- Condition modules ---------- */
static void motion_sickness(Result *r) {
    strcpy(r->condition, "Space motion sickness");
    int day = ask_int("Days since symptoms started", 0, 30);
    int vomits = ask_int("Times vomited in last 24h", 0, 20);
    int fluids = ask_yn("Able to keep fluids down?");
    WHY(r, "Common in the first days (60-80%% of astronauts). ");
    r->lvl = SELF_TREAT;
    STEP(r, "1. Limit sudden head movements; rest in a stable position.\n");
    STEP(r, "2. Sip water or electrolyte drink slowly.\n");
    if (has("anti-nausea tablets")) STEP(r, "3. Use on-board anti-nausea tablets per the kit protocol card.\n");
    else STEP(r, "3. No anti-nausea tablets left in kit: contact ground.\n");
    if (vomits > 3 || !fluids) {
        raise_to(r, CONTACT_GROUND); WHY(r, "Repeated vomiting or unable to keep fluids (dehydration risk). ");
        if (has("IV fluids")) STEP(r, "4. Prepare IV fluids only on ground instruction.\n");
    } else if (day > 5) { raise_to(r, CONTACT_GROUND); WHY(r, "Symptoms lasting more than 5 days. "); }
    else if (day > 3)   { raise_to(r, MONITOR);        WHY(r, "Symptoms lasting beyond the usual first days. "); }
}

static void kidney_urinary(Result *r) {
    strcpy(r->condition, "Kidney stone / urinary problem");
    int pain = ask_int("Pain severity", 0, 10);
    int flank = ask_yn("Pain in side/back or lower abdomen?");
    int blood = ask_yn("Blood in urine?");
    int burn = ask_yn("Burning when urinating?");
    int fever = ask_yn("Fever or chills?");
    int vomit = ask_yn("Nausea or vomiting?");
    int relief = 1;
    if (pain >= 4 && has("painkillers")) relief = ask_yn("Did the on-board painkillers help?");
    WHY(r, "Kidney stones rank as a leading cause of emergency evacuation from the ISS. ");
    r->lvl = SELF_TREAT;
    STEP(r, "1. Drink water steadily unless told otherwise; record fluid intake.\n");
    if (has("painkillers")) STEP(r, "2. Use on-board painkillers exactly per the kit protocol card.\n");
    else { STEP(r, "2. No painkillers in kit.\n"); raise_to(r, CONTACT_GROUND); }
    if (has("thermometer")) STEP(r, "3. Check temperature with the thermometer and log it.\n");
    if (has("ultrasound") && flank) STEP(r, "4. Ground may request an ultrasound scan of the kidney area.\n");
    if (burn && has("antibiotics")) STEP(r, "5. Antibiotics only on ground instruction.\n");
    if (flank && pain >= 4) { raise_to(r, MONITOR); WHY(r, "Flank pain with moderate severity. "); }
    if (pain >= 7 || !relief || blood || vomit) {
        raise_to(r, CONTACT_GROUND); WHY(r, "Severe pain, blood, vomiting or painkillers not working. ");
    }
    if (fever && (flank || pain >= 5)) {
        raise_to(r, EMERGENCY); WHY(r, "Fever with pain suggests possible infection. ");
    }
}

static void blood_clot(Result *r) {
    strcpy(r->condition, "Possible blood clot");
    int site = ask_yn("Swelling, redness or warmth in neck, arm or leg?");
    int pain = ask_int("Pain severity in that area", 0, 10);
    int head = ask_yn("New headache or facial swelling?");
    WHY(r, "Clots can be silent (a 2020 ISS case had no symptoms). ");
    r->lvl = CONTACT_GROUND;
    STEP(r, "1. Do not massage the area. Keep the patient calm.\n");
    if (has("ultrasound")) STEP(r, "2. Prepare the ultrasound; ground may guide a vein scan.\n");
    if (has("blood pressure cuff")) STEP(r, "3. Record blood pressure and pulse.\n");
    STEP(r, "4. Blood thinners only on ground instruction.\n");
    if (site && (pain >= 6 || head)) { r->lvl = EMERGENCY; WHY(r, "Swelling with severe pain or head/face symptoms. "); }
    else if (!site && pain < 3 && !head) { r->lvl = MONITOR; WHY(r, "No local signs. Routine ultrasound check advised. "); }
}

static void vision(Result *r) {
    strcpy(r->condition, "Vision changes (SANS)");
    int sudden = ask_yn("Sudden vision loss or flashes?");
    int blur = ask_yn("Blurry near vision or focus change?");
    int head = ask_yn("Headache with the vision change?");
    WHY(r, "Fluid shifts toward the head raise pressure (about 70%% of long-mission crew show some change). ");
    if (sudden) { r->lvl = EMERGENCY; WHY(r, "Sudden vision loss. "); STEP(r, "1. Protect the eyes, stop tasks, alert the crew.\n"); return; }
    r->lvl = MONITOR;
    STEP(r, "1. Log the change daily with an eye chart check.\n");
    if (has("eye drops")) STEP(r, "2. Lubricating eye drops may ease dryness.\n");
    if (has("ultrasound")) STEP(r, "3. Ground may ask for an eye ultrasound.\n");
    if (blur && head) { r->lvl = CONTACT_GROUND; WHY(r, "Blur plus headache may indicate raised pressure. "); }
}

static void sleep_mental(Result *r) {
    strcpy(r->condition, "Sleep / mental health");
    int days = ask_int("Days of poor sleep or low mood", 0, 60);
    int func = ask_yn("Is it affecting your ability to work or perform tasks?");
    WHY(r, "Isolation, confinement and stress raise risk of anxiety and low morale. ");
    r->lvl = (days > 2) ? MONITOR : SELF_TREAT;
    STEP(r, "1. Keep a fixed sleep schedule and dim lights before sleep.\n");
    STEP(r, "2. Schedule exercise and a call with family or a crew chat.\n");
    if (days >= 7 || func) { raise_to(r, CONTACT_GROUND); WHY(r, "Persisting more than a week or affecting duties. "); }
}

static void illness(Result *r) {
    strcpy(r->condition, "Illness / immunity");
    float temp = 0; char line[32];
    if (has("thermometer")) { printf("Temperature in C (0 if unknown): "); if (fgets(line, sizeof line, stdin)) sscanf(line, "%f", &temp); }
    int days = ask_int("Days of illness", 0, 30);
    int rash = ask_yn("New rash or allergic reaction?");
    WHY(r, "Stress hormones in space can weaken immunity. ");
    r->lvl = SELF_TREAT;
    STEP(r, "1. Rest, drink fluids, and isolate if possible.\n");
    if (has("antibiotics")) STEP(r, "2. Antibiotics only on ground instruction.\n");
    if (temp >= 38.0f || days >= 3) { raise_to(r, MONITOR); WHY(r, "Fever or illness lasting several days. "); }
    if (temp >= 39.0f || days >= 5 || rash) { raise_to(r, CONTACT_GROUND); WHY(r, "High fever, long illness or rash. "); }
}

/* ---------- Ground report + log ---------- */
static void save_report(const Result *r) {
    time_t t = time(NULL); char ts[32];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
    FILE *f = fopen("ground_report.txt", "w");
    if (f) {
        fprintf(f, "GROUND LINK REPORT (prototype)\nTime: %s\nCondition: %s\nTriage: %s\nReason: %s\n\nSteps given:\n%s\n"
                   "Note: Message may take ~20 minutes round trip. Keep patient monitored.\n",
                ts, r->condition, LEVEL_NAME[r->lvl], r->reason, r->steps);
        fclose(f);
    }
    f = fopen("symptom_log.txt", "a");
    if (f) { fprintf(f, "%s | %s | %s\n", ts, r->condition, LEVEL_NAME[r->lvl]); fclose(f); }
}

static void print_log(void) {
    FILE *f = fopen("symptom_log.txt", "r"); char line[256];
    printf("\nSYMPTOM LOG\n");
    if (!f) { printf("  (empty)\n"); return; }
    while (fgets(line, sizeof line, f)) printf("  %s", line);
    fclose(f);
}

int main(void) {
    printf("=== AstroMedic AI (prototype) ===\nNot a substitute for flight surgeon advice.\n");
    for (;;) {
        printf("\nWhat is the problem?\n"
               " 1) Nausea / dizziness (space motion sickness)\n"
               " 2) Side/back pain or urinary problem (kidney stone / UTI)\n"
               " 3) Swelling or pain in neck/limb (possible clot)\n"
               " 4) Vision changes\n"
               " 5) Sleep / mood\n"
               " 6) Fever / feeling ill\n"
               " 7) Show kit    8) Show symptom log    0) Exit\n> ");
        int c = ask_int("Choice", 0, 8);
        if (c == 0) break;
        if (c == 7) { show_kit(); continue; }
        if (c == 8) { print_log(); continue; }
        Result r; memset(&r, 0, sizeof r);
        if (!red_flags(&r)) {
            switch (c) {
                case 1: motion_sickness(&r); break;
                case 2: kidney_urinary(&r);  break;
                case 3: blood_clot(&r);      break;
                case 4: vision(&r);          break;
                case 5: sleep_mental(&r);    break;
                case 6: illness(&r);         break;
            }
        }
        printf("\n========== RESULT ==========\nCondition: %s\nTRIAGE: %s\n", r.condition, LEVEL_NAME[r.lvl]);
        if (r.lvl >= CONTACT_GROUND) printf("*** CONTACT EARTH: ground report saved to ground_report.txt ***\n");
        printf("Why: %s\n\nFirst-aid steps (kit-aware):\n%s", r.reason, r.steps);
        save_report(&r);
        printf("\nPrototype only. Follow flight surgeon instructions.\n");
    }
    return 0;
}
