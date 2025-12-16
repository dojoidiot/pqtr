// note.cpp - Note event system

#include "desk.hpp"
#include <cstring>

// Globals
Note g_notes[MAX_NOTES];
int g_note_count = 0;
Note g_note_history[MAX_NOTE_HISTORY];
int g_note_history_count = 0;
bool g_note_history_scroll = false;

void postNote(const char* event, const char* data) {
    // Add to active queue
    if (g_note_count >= MAX_NOTES) {
        for (int i = 0; i < MAX_NOTES - 1; i++) {
            g_notes[i] = g_notes[i + 1];
        }
        g_note_count = MAX_NOTES - 1;
    }
    Note& note = g_notes[g_note_count++];
    strncpy(note.event, event, sizeof(note.event) - 1);
    strncpy(note.data, data, sizeof(note.data) - 1);

    // Add to history
    if (g_note_history_count >= MAX_NOTE_HISTORY) {
        for (int i = 0; i < MAX_NOTE_HISTORY - 1; i++) {
            g_note_history[i] = g_note_history[i + 1];
        }
        g_note_history_count = MAX_NOTE_HISTORY - 1;
    }
    Note& hist = g_note_history[g_note_history_count++];
    strncpy(hist.event, event, sizeof(hist.event) - 1);
    strncpy(hist.data, data, sizeof(hist.data) - 1);
    g_note_history_scroll = true;
}

bool checkNote(const char* event, char* data_out, size_t data_size) {
    for (int i = 0; i < g_note_count; i++) {
        if (strcmp(g_notes[i].event, event) == 0) {
            if (data_out && data_size > 0) {
                strncpy(data_out, g_notes[i].data, data_size - 1);
                data_out[data_size - 1] = '\0';
            }
            for (int j = i; j < g_note_count - 1; j++) {
                g_notes[j] = g_notes[j + 1];
            }
            g_note_count--;
            return true;
        }
    }
    return false;
}
