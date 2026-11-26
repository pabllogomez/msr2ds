// msr-native's achievements (achievements.cpp): local only, no network and no account. The list (achievements.txt),
// what the game shows read on the main thread every 2 s with the Discord presence's, the unlocked ones kept in
// user/msr/achievements.ini and a notice drawn over the game by the overlay (src/msr/overlay.cpp).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void msr_achievements_start(void);                 /* main.c, after the settings: the list and the progress read */
void msr_achievements_scenario(const char *path);  /* layered_fs.cpp: a package read whole (packs/maps/<scenario>/...) */
void msr_achievements_tick(void);                  /* main.c, every 2 s on the main thread: what unlocks now */
/* src/msr/overlay.cpp, each frame of the overlay over the game: the newest unlock's notice (scale: the menu's) */
void msr_achievements_draw(float scale, float width, float height);

#ifdef __cplusplus
}
#endif
