#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#include "core.h"

// Callback for CURL to write response data
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **response_ptr = (char**)userp;
    
    *response_ptr = realloc(*response_ptr, realsize + 1);
    if(*response_ptr == NULL) {
        return 0;
    }
    
    memcpy(*response_ptr, contents, realsize);
    (*response_ptr)[realsize] = 0;
    return realsize;
}

void combo_set_objectives(ComboState* state, Objective* objectives, uint32_t count) {
    if (state->objectives) {
        // Free existing objectives
        for (uint32_t i = 0; i < state->objective_count; i++) {
            free(state->objectives[i].name);
            free(state->objectives[i].description);
        }
        free(state->objectives);
    }
    
    state->objectives = malloc(sizeof(Objective) * count);
    state->objective_count = count;
    state->active_objective_index = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        state->objectives[i] = objectives[i];
        state->objectives[i].name = strdup(objectives[i].name);
        state->objectives[i].description = strdup(objectives[i].description);
    }
}

void combo_switch_objective(ComboState* state, uint32_t index) {
    if (index < state->objective_count) {
        state->active_objective_index = index;
    }
}

void combo_update_objective_progress(ComboState* state, uint32_t score_increment) {
    if (state->objective_count == 0) return;
    
    Objective* current = &state->objectives[state->active_objective_index];
    current->current_score += score_increment;
    
    if (current->current_score >= current->target_score) {
        current->completed = true;
    }
}

void combo_sync_objectives(ComboState* state, const char* server_url) {
    CURL *curl;
    CURLcode res;
    char *response = NULL;
    
    curl = curl_easy_init();
    if (!curl) return;
    
    // Setup CURL request
    curl_easy_setopt(curl, CURLOPT_URL, server_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK && response) {
        json_error_t error;
        json_t *root = json_loads(response, 0, &error);
        
        if (root && json_is_array(root)) {
            size_t count = json_array_size(root);
            Objective *objectives = malloc(sizeof(Objective) * count);
            
            for (size_t i = 0; i < count; i++) {
                json_t *obj = json_array_get(root, i);
                objectives[i].name = strdup(json_string_value(json_object_get(obj, "name")));
                objectives[i].description = strdup(json_string_value(json_object_get(obj, "description")));
                objectives[i].target_score = json_integer_value(json_object_get(obj, "target_score"));
                objectives[i].current_score = 0;
                objectives[i].completed = false;
            }
            
            combo_set_objectives(state, objectives, count);
            free(objectives);
        }
        
        json_decref(root);
    }
    
    free(response);
    curl_easy_cleanup(curl);
}

void combo_sync_with_server(ComboState* state, const char* server_url) {
    CURL *curl;
    CURLcode res;
    
    curl = curl_easy_init();
    if (!curl) return;
    
    // Create JSON payload
    json_t *root = json_object();
    json_t *objectives = json_array();
    
    for (uint32_t i = 0; i < state->objective_count; i++) {
        json_t *obj = json_object();
        json_object_set_new(obj, "name", json_string(state->objectives[i].name));
        json_object_set_new(obj, "current_score", json_integer(state->objectives[i].current_score));
        json_object_set_new(obj, "completed", json_boolean(state->objectives[i].completed));
        json_array_append_new(objectives, obj);
    }
    
    json_object_set_new(root, "objectives", objectives);
    char *payload = json_dumps(root, JSON_COMPACT);
    
    // Setup CURL request
    curl_easy_setopt(curl, CURLOPT_URL, server_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    res = curl_easy_perform(curl);
    
    // Cleanup
    curl_slist_free_all(headers);
    free(payload);
    json_decref(root);
    curl_easy_cleanup(curl);
} 