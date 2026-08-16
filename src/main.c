#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <json-c/json_object.h>
#include <json-c/json_types.h>
#include <qlibc/containers/qvector.h>
#include <qlibc/qlibc.h>
#include "json-c/json_util.h"

#define MAX_LEN 16
#define CACHE_PATH "/dev/shm/polybar-spotify-playing-cache-c.txt"
#define TITLE_CYCLE_INDEX_KEY "titleCycle.index"
#define ARTIST_CYCLE_INDEX_KEY "artistCycle.index"
#define TITLE_CYCLE_STEPS_KEY "titleCycle.steps"
#define ARTIST_CYCLE_STEPS_KEY "artistCycle.steps"
typedef enum {
  SONG_TITLE,
  SONG_ARTIST
} song_attr;

bool is_whitespace(char c, bool include_space) {
  if (include_space)
    return (c == '\t' || c == '\r' || c == '\n' || c == ' ');
  else
    return (c == '\t' || c == '\r' || c == '\n');
}
// Remove whitespace chars from string
// *dest has to be free'd
int strip_string(char **dest, const char *src) {
  *dest = calloc(strlen(src), sizeof(char));
  if (strlen(src) < 1) {
    return 0;
  }

  unsigned int start_i = 0, end_i = strlen(src) - 1;
  for (unsigned long i = 0; i < strlen(src); i++) {
    if (!is_whitespace(src[i], true)) {
      start_i = i;
      break;
    }
  }

  for (unsigned long i = strlen(src) - 1; i >= start_i; i--) {
    if (!is_whitespace(src[i], true)) {
      // Ending boundary is exclusive by convention
      end_i = i + 1;
      break;
    }
  }

  assert(end_i > start_i);
  strncpy(*dest, &src[start_i], end_i - start_i);

  return strlen(*dest);
}

// Execute a shell command and set `*output` to the first line of output
// Returns exit status of cmd
// *output needs to be free'd
int exec_single_line(char **output, const char *cmd) {
  FILE *cmd_f = popen(cmd, "r");
  assert(cmd_f != NULL);

  size_t line_size = 0;
  char *lineout = NULL;
  ssize_t n = getline(&lineout, &line_size, cmd_f);
  if (n <= 0) {
    pclose(cmd_f);
    return -1;
  }
  strip_string(output, lineout);
  free(lineout);
  return pclose(cmd_f);
}

// Provide steps to animate a string with length > max_len
// Result allocated and set in *vector
// Each string in the vector has to be free'd
void string_cycle_steps(char **s, const size_t max_len, qvector_t **vector) {
  *vector = NULL;
  if (strlen(*s) == 0) {
    fprintf(stderr, "WARN: Passed empty string to string_cycle_steps()\n");
    return;
  }

  *vector = qvector(16, sizeof(char*), QVECTOR_RESIZE_LINEAR);
  if (strlen(*s) <= max_len) {
    assert((*vector)->addlast(*vector, s));
    return;
  }

  for (unsigned long i = 0; i < strlen(*s) - max_len + 1; i++) {
    char *sub1 = calloc(max_len + 1, sizeof(char));
    strncpy(sub1, *s + i, max_len);
    strcat(sub1, "\0");
    assert((*vector)->addlast(*vector, &sub1));
  }
}

// Generates and stores animation steps in json object
// Returns first string of the animation
// Returned char* needs to be free'd
char* json_gen_animation_steps(json_object *jo, song_attr attr, char *full_string) {
  int add_res = -1;
  if (attr == SONG_TITLE) {
    add_res = json_object_object_add(jo, TITLE_CYCLE_INDEX_KEY,
                                     json_object_new_int(0));
  } else {
    add_res = json_object_object_add(jo, ARTIST_CYCLE_INDEX_KEY,
                                     json_object_new_int(0));
  }
  assert(add_res == 0);
  qvector_t *v_steps = NULL; // vector of char *
  string_cycle_steps(&full_string, MAX_LEN, &v_steps);
  assert(v_steps != NULL);
  size_t n_steps = qvector_size(v_steps);
  assert(n_steps > 0);
  json_object *jo_steps_arr = json_object_new_array_ext((int)n_steps);
  assert(jo_steps_arr != NULL);
  for (unsigned long i = 0; i < n_steps; i++) {
    char *s_str = *(char**)v_steps->getat(v_steps, i, false);
    json_object_array_add(jo_steps_arr,
                          json_object_new_string(s_str));
  }
  if (attr == SONG_TITLE) {
    add_res =
        json_object_object_add(jo, TITLE_CYCLE_STEPS_KEY, jo_steps_arr);
  } else {
    add_res =
        json_object_object_add(jo, ARTIST_CYCLE_STEPS_KEY, jo_steps_arr);
  }
  assert(add_res == 0);
  char* first = *(char**)v_steps->getat(v_steps, 0, false);
  assert(first != NULL);
  char *ret = calloc(strlen(first) + 1, sizeof(char));
  strncpy(ret, first, strlen(first));
  for (unsigned long i = 0; i < n_steps; i++) {
    free(*(char**)v_steps->getat(v_steps, i, false));
  }
  qvector_free(v_steps);
  return ret;
}

// Get next animation step from existing json and increment index in-place
// Returns a string or NULL if no step found
// Returned string needs to be free'd
char *json_next_animation_step(json_object *ji, song_attr attr) {
  json_object *jo_cycle_index = NULL, *jo_cycle_steps = NULL;

  short found_cycle_index = -1, found_cycle_steps = -1;
  if (attr == SONG_TITLE) {
    found_cycle_index = json_object_object_get_ex(ji, TITLE_CYCLE_INDEX_KEY,
                                                  &jo_cycle_index);
    found_cycle_steps = json_object_object_get_ex(ji, TITLE_CYCLE_STEPS_KEY,
                                                  &jo_cycle_steps);
  } else {
    found_cycle_index = json_object_object_get_ex(ji, ARTIST_CYCLE_INDEX_KEY,
                                                  &jo_cycle_index);
    found_cycle_steps = json_object_object_get_ex(ji, ARTIST_CYCLE_STEPS_KEY,
                                                  &jo_cycle_steps);
  }
  if (found_cycle_index == 1 && found_cycle_steps == 1) {
    unsigned int cycle_index =
        json_object_get_int(jo_cycle_index);
    assert(cycle_index != 0 || errno != EINVAL);
    int new_cycle_index;
    if (cycle_index >=
        json_object_array_length(jo_cycle_steps) - 1) {
      new_cycle_index = 0;
    } else {
      new_cycle_index = cycle_index + 1;
    }
    json_object *jo_cycle_step = json_object_array_get_idx(
        jo_cycle_steps, new_cycle_index);
    assert(jo_cycle_step != NULL);
    const char *step = json_object_get_string(jo_cycle_step);
    assert(step != NULL);

    // Increment index inside json
    assert(json_object_set_int(jo_cycle_index, new_cycle_index) == 1);

    char *ret = calloc(strlen(step) + 1, sizeof(char));
    assert(ret != NULL);
    strncpy(ret, step, strlen(step));
    return ret;
  }
  return NULL;
}

int main(void) {
  char *full_title = NULL, *full_artist = NULL;
  int title_cmd_status = exec_single_line(
      &full_title, "playerctl -f {{title}} -p spotify metadata 2>&1");
  int artist_cmd_status = exec_single_line(
      &full_artist, "playerctl -f {{artist}} -p spotify metadata 2>&1");
  if (full_title == NULL || full_artist == NULL) {
    return EXIT_FAILURE;
  }
  if ((title_cmd_status != 0 && strcmp(full_title, "No players found") == 0) ||
      (artist_cmd_status != 0 && strcmp(full_artist, "No players found") == 0)) {
    printf("\n");
    return EXIT_SUCCESS;
  }
  else if (title_cmd_status != 0 || artist_cmd_status != 0)
    return EXIT_FAILURE;

  // Both values < MAX_LEN, output them as is and exit
  if (strlen(full_title) < MAX_LEN && strlen(full_artist) < MAX_LEN) {
    printf("%s - %s\n", full_title, full_artist);
    json_object *json_empty = json_object_new_object();
    if (json_object_to_file(CACHE_PATH, json_empty) < 0) {
      fprintf(stderr, "Failed to write JSON to cache file\n%s\n",
              json_util_get_last_err());
    }
    free(full_title);
    free(full_artist);
    return EXIT_SUCCESS;
  }

  // Read existing cache
  if (access(CACHE_PATH, R_OK) == 0) {
    json_object *json_in = json_object_from_file(CACHE_PATH);
    if (json_in != NULL) {
      json_object *jo_full_title = NULL;
      json_object *jo_full_artist = NULL;
      short found_ft =
          json_object_object_get_ex(json_in, "fullTitle", &jo_full_title);
      short found_fa =
          json_object_object_get_ex(json_in, "fullArtist", &jo_full_artist);
      const char *cache_full_title = NULL;
      cache_full_title = json_object_get_string(jo_full_title);
      const char *cache_full_artist = NULL;
      cache_full_artist = json_object_get_string(jo_full_artist);

      if (found_ft == 1 && found_fa == 1 &&
          strcmp(full_title, cache_full_title) == 0 &&
          strcmp(full_artist, cache_full_artist) == 0) {
        // Same song, animate title and/or artist
        char *part_title = NULL, *part_artist = NULL;
        part_title = json_next_animation_step(json_in, SONG_TITLE);
        part_artist = json_next_animation_step(json_in, SONG_ARTIST);

        // Write any changes to JSON cache
        if (part_title != NULL || part_artist != NULL)
          assert(json_object_to_file(CACHE_PATH, json_in) >= 0);

        printf("%s - %s\n", part_title == NULL ? full_title : part_title,
               part_artist == NULL ? full_artist : part_artist);

        json_object_put(json_in);
        free(part_title); // Free'ing NULL is OK
        free(part_artist); // Free'ing NULL is OK
        free(full_title);
        free(full_artist);
        return EXIT_SUCCESS;
      }
    } else {
      fprintf(stderr, "Failed to parse JSON from cache file\n%s\n",
              json_util_get_last_err());
      // Continue as if cache didn't exist
    }
    json_object_put(json_in);
  }

  // Cache didn't exist or existed but current song didn't match
  // Song changed, generate new cache
  json_object *json_out = json_object_new_object();

  int add_res = json_object_object_add(
      json_out, "fullTitle", json_object_new_string(full_title));
  assert(add_res == 0);
  add_res = json_object_object_add(
      json_out, "fullArtist", json_object_new_string(full_artist));
  assert(add_res == 0);
  char* title_out = NULL;
  title_out = full_title;
  char* artist_out = NULL;
  artist_out = full_artist;

  if (strlen(full_title) > MAX_LEN) {
    title_out = json_gen_animation_steps(json_out, SONG_TITLE, full_title);
    assert(title_out != NULL);
  }
  if (strlen(full_artist) > MAX_LEN) {
    artist_out = json_gen_animation_steps(json_out, SONG_ARTIST, full_artist);
    assert(artist_out != NULL);
  }

  // Print result to stdout
  printf("%s - %s\n", title_out, artist_out);

  // Write next animation steps to cache
  if (json_object_to_file(CACHE_PATH, json_out) < 0) {
    fprintf(stderr, "Failed to write JSON to cache file\n%s\n",
            json_util_get_last_err());
  }

  free(title_out);
  free(artist_out);
  json_object_put(json_out);
  free(full_title);
  free(full_artist);
  return EXIT_SUCCESS;
}
