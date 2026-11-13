#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_EMERGENCIES 100
#define MAX_STACK 100
#define NAME_LEN 50
#define LOC_LEN 100
#define GRID_ROWS 5
#define GRID_COLS 5
#define NUM_GRAPH_NODES 8
#define HASH_SIZE 31
#define INF 99999

// ===============================
// DATA STRUCTURES (SYLLABUS SEC 1 - 6)
// ===============================

// Section 1: 2D Array Cell for Satellite Spectral Telemetry
typedef struct SectorCell {
    int row, col;
    char sectorCode[16];     // e.g., "IDN-KLM1"
    char areaName[LOC_LEN];  // e.g., "Sebangau_Peat_Kalimantan"
    double lat, lon;
    float ndvi;              // Normalized Difference Vegetation Index (-1 to +1)
    float ndviBaseline;      // Pre-fire baseline NDVI
    float ndwi;              // Peatland Moisture / Water Index (<-0.25 = Dry Peat)
    float thermalK;          // VIIRS Brightness Temp in Kelvin (>340K = Hotspot)
    float severityScore;     // Computed 0.0 - 100.0
    int mappedGraphNode;     // Links this satellite pixel to the Kalimantan Road Graph
} SectorCell;

// Section 1: 1D Array Struct for Active Emergencies
typedef struct Emergency {
    int id;
    char sectorCode[16];
    char type[NAME_LEN];
    char location[LOC_LEN];
    double lat, lon;
    float ndvi;
    float ndwi;
    float thermalK;
    float severityScore;
    int priority;            // 1 = low, 5 = critical
    int targetGraphNode;
} Emergency;

// Section 3: Singly Linked List (Rescue Units) & Circular Linked List (Satellites)
typedef struct SNode {
    char resource[NAME_LEN];
    struct SNode *next;
} SNode;

typedef struct CNode {
    char satName[NAME_LEN];
    char sensorType[NAME_LEN];
    struct CNode *next;
} CNode;

// Section 4: Binary Search Tree (Indexed by Spectral Severity Score)
typedef struct BSTNode {
    char sectorCode[16];
    char areaName[LOC_LEN];
    float severityScore;
    float ndvi, ndwi, thermalK;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

// Section 6: Hash Table Entry with Linear Probing
typedef struct HashEntry {
    int isOccupied;
    char sectorCode[16];
    char areaName[LOC_LEN];
    double lat, lon;
    float ndvi, ndwi, thermalK;
    float severityScore;
    int probes;
} HashEntry;

// ===============================
// GLOBAL STATE
// ===============================
SectorCell sectorGrid[GRID_ROWS][GRID_COLS]; // Sec 1: 2D Array
Emergency emergencies[MAX_EMERGENCIES];      // Sec 1: 1D Array
int emergencyCount = 0;
int nextId = 1001;

int historyStack[MAX_STACK];                 // Sec 3: Stack
int stackTop = -1;

SNode *singlyHead = NULL;                    // Sec 3: Singly Linked List
SNode *singlyTail = NULL;
CNode *circularHead = NULL;                  // Sec 3: Circular Linked List
CNode *activeSatellite = NULL;

BSTNode *bstRoot = NULL;                     // Sec 4: BST Root

// Sec 5: Graph (Central Kalimantan & Indonesia Command Network)
const char *graphNodeNames[NUM_GRAPH_NODES] = {
    "Palangka_Raya_BNPB_Base",  // Node 0 (Origin Airbase)
    "Tumbang_Nusa_Peat_Bridge", // Node 1 (Smoke/Peat Fire Choke Point)
    "Pulang_Pisau_Corridor",    // Node 2 (Peatland Fire Corridor)
    "Katingan_West_Depot",      // Node 3 (Safe Western Bypass)
    "Sebangau_National_Park",   // Node 4 (Major Fire Zone - 22,595 ha center)
    "Banjarmasin_South_Port",   // Node 5 (Southern Logistics Hub)
    "Sampit_Staging_Hub",       // Node 6 (Safe Southern Detour Hub)
    "Kapuas_East_Sector"        // Node 7 (Eastern Peat Basin)
};

double graphNodeCoords[NUM_GRAPH_NODES][2] = {
    {-2.2136, 113.9108}, // 0: Palangka Raya Base
    {-2.3850, 114.0920}, // 1: Tumbang Nusa Bridge
    {-2.7540, 114.2600}, // 2: Pulang Pisau
    {-2.0650, 113.4000}, // 3: Katingan Depot
    {-2.5800, 113.8800}, // 4: Sebangau National Park
    {-3.3194, 114.5908}, // 5: Banjarmasin Port
    {-2.5320, 112.9500}, // 6: Sampit Hub
    {-2.9800, 114.3900}  // 7: Kapuas Sector
};

int baseAdjMatrix[NUM_GRAPH_NODES][NUM_GRAPH_NODES];
int liveAdjMatrix[NUM_GRAPH_NODES][NUM_GRAPH_NODES];
int nodeBlocked[NUM_GRAPH_NODES];
int lastShortestPath[NUM_GRAPH_NODES];
int lastPathLen = 0;
int lastRouteTarget = 4;
int lastRouteCost = 0;

HashEntry hashTable[HASH_SIZE];              // Sec 6: Hash Table


void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

void readLine(char *text, int size) {
    if (fgets(text, size, stdin) != NULL) {
        text[strcspn(text, "\n")] = '\0';
    }
}


void initHashTable(void) {
    for (int i = 0; i < HASH_SIZE; i++) {
        hashTable[i].isOccupied = 0;
        hashTable[i].probes = 0;
    }
}

int hashFunction(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return (int)(hash % HASH_SIZE);
}

void insertHashTable(SectorCell cell) {
    int idx = hashFunction(cell.sectorCode);
    int startIdx = idx;
    int probes = 1;

    while (hashTable[idx].isOccupied && strcmp(hashTable[idx].sectorCode, cell.sectorCode) != 0) {
        idx = (idx + 1) % HASH_SIZE;
        probes++;
        if (idx == startIdx) return;
    }

    hashTable[idx].isOccupied = 1;
    strncpy(hashTable[idx].sectorCode, cell.sectorCode, 15);
    strncpy(hashTable[idx].areaName, cell.areaName, LOC_LEN - 1);
    hashTable[idx].lat = cell.lat;
    hashTable[idx].lon = cell.lon;
    hashTable[idx].ndvi = cell.ndvi;
    hashTable[idx].ndwi = cell.ndwi;
    hashTable[idx].thermalK = cell.thermalK;
    hashTable[idx].severityScore = cell.severityScore;
    hashTable[idx].probes = probes;
}

void searchHashTable(void) {
    char code[16];
    printf("Enter Hotspot Sector Code (e.g., IDN-KLM1): ");
    clearInputBuffer();
    readLine(code, 16);

    int idx = hashFunction(code);
    int startIdx = idx;
    int probes = 1;

    while (hashTable[idx].isOccupied) {
        if (strcmp(hashTable[idx].sectorCode, code) == 0) {
            printf("\n[HASH HIT in %d probe(s) at Bucket %d]\n", probes, idx);
            printf("Sector: %s (%s) | GPS: %.4f, %.4f\n",
                   hashTable[idx].sectorCode, hashTable[idx].areaName,
                   hashTable[idx].lat, hashTable[idx].lon);
            printf("NDVI: %.2f | NDWI: %.2f | Thermal: %.1fK | Severity: %.1f\n",
                   hashTable[idx].ndvi, hashTable[idx].ndwi,
                   hashTable[idx].thermalK, hashTable[idx].severityScore);
            return;
        }
        idx = (idx + 1) % HASH_SIZE;
        probes++;
        if (idx == startIdx) break;
    }
    printf("Sector '%s' not found in Hash Table.\n", code);
}

void displayHashTable(void) {
    printf("\n========== HASH TABLE (799 HOTSPOT TELEMETRY REGISTRY) ==========\n");
    printf("%-6s %-12s %-28s %-10s %-6s\n", "Bucket", "SectorCode", "Hotspot Zone", "Severity", "Probes");
    printf("---------------------------------------------------------------------\n");
    for (int i = 0; i < HASH_SIZE; i++) {
        if (hashTable[i].isOccupied) {
            printf("%-6d %-12s %-28s %-10.1f %-6d\n",
                   i, hashTable[i].sectorCode, hashTable[i].areaName,
                   hashTable[i].severityScore, hashTable[i].probes);
        }
    }
}

// ===============================
// SECTION 4: BINARY SEARCH TREE (BST)
// ===============================
BSTNode* insertBST(BSTNode *root, SectorCell cell) {
    if (root == NULL) {
        BSTNode *node = (BSTNode *)malloc(sizeof(BSTNode));
        strncpy(node->sectorCode, cell.sectorCode, 15);
        strncpy(node->areaName, cell.areaName, LOC_LEN - 1);
        node->severityScore = cell.severityScore;
        node->ndvi = cell.ndvi;
        node->ndwi = cell.ndwi;
        node->thermalK = cell.thermalK;
        node->left = node->right = NULL;
        return node;
    }
    if (cell.severityScore < root->severityScore) {
        root->left = insertBST(root->left, cell);
    } else {
        root->right = insertBST(root->right, cell);
    }
    return root;
}

void inorderBST(BSTNode *root) {
    if (root == NULL) return;
    inorderBST(root->left);
    printf("  %-10s | %-28s | Severity: %5.1f | Temp: %.1fK | NDVI: %5.2f | NDWI: %5.2f\n",
           root->sectorCode, root->areaName, root->severityScore,
           root->thermalK, root->ndvi, root->ndwi);
    inorderBST(root->right);
}

void freeBST(BSTNode *root) {
    if (root == NULL) return;
    freeBST(root->left);
    freeBST(root->right);
    free(root);
}

// ===============================
// SECTION 5: GRAPH & DIJKSTRA ROUTING
// ===============================
void initKalimantanGraph(void) {
    for (int i = 0; i < NUM_GRAPH_NODES; i++) {
        nodeBlocked[i] = 0;
        for (int j = 0; j < NUM_GRAPH_NODES; j++) {
            baseAdjMatrix[i][j] = (i == j) ? 0 : INF;
        }
    }
    // Kalimantan Highway & Firebreak Distances (in km)
    baseAdjMatrix[0][1] = baseAdjMatrix[1][0] = 28; // Palangka Raya <-> Tumbang Nusa
    baseAdjMatrix[0][2] = baseAdjMatrix[2][0] = 35; // Palangka Raya <-> Pulang Pisau
    baseAdjMatrix[0][3] = baseAdjMatrix[3][0] = 42; // Palangka Raya <-> Katingan Depot
    baseAdjMatrix[1][4] = baseAdjMatrix[4][1] = 24; // Tumbang Nusa <-> Sebangau Park
    baseAdjMatrix[1][7] = baseAdjMatrix[7][1] = 45; // Tumbang Nusa <-> Kapuas
    baseAdjMatrix[2][4] = baseAdjMatrix[4][2] = 30; // Pulang Pisau <-> Sebangau Park
    baseAdjMatrix[2][5] = baseAdjMatrix[5][2] = 55; // Pulang Pisau <-> Banjarmasin
    baseAdjMatrix[3][6] = baseAdjMatrix[6][3] = 38; // Katingan <-> Sampit Hub
    baseAdjMatrix[6][4] = baseAdjMatrix[4][6] = 34; // Sampit Hub <-> Sebangau Park
    baseAdjMatrix[4][5] = baseAdjMatrix[5][4] = 62; // Sebangau <-> Banjarmasin
    baseAdjMatrix[5][7] = baseAdjMatrix[7][5] = 40; // Banjarmasin <-> Kapuas

    memcpy(liveAdjMatrix, baseAdjMatrix, sizeof(baseAdjMatrix));
}

void runDijkstra(int src, int dest, int verbose) {
    int dist[NUM_GRAPH_NODES];
    int visited[NUM_GRAPH_NODES];
    int parent[NUM_GRAPH_NODES];

    for (int i = 0; i < NUM_GRAPH_NODES; i++) {
        dist[i] = INF;
        visited[i] = 0;
        parent[i] = -1;
    }
    dist[src] = 0;

    for (int count = 0; count < NUM_GRAPH_NODES - 1; count++) {
        int min = INF, u = -1;
        for (int v = 0; v < NUM_GRAPH_NODES; v++) {
            if (!visited[v] && dist[v] <= min) {
                min = dist[v];
                u = v;
            }
        }
        if (u == -1) break;
        visited[u] = 1;

        for (int v = 0; v < NUM_GRAPH_NODES; v++) {
            if (!visited[v] && liveAdjMatrix[u][v] != INF &&
                dist[u] + liveAdjMatrix[u][v] < dist[v]) {
                parent[v] = u;
                dist[v] = dist[u] + liveAdjMatrix[u][v];
            }
        }
    }

    lastPathLen = 0;
    int curr = dest;
    int tempPath[NUM_GRAPH_NODES];
    while (curr != -1) {
        tempPath[lastPathLen++] = curr;
        curr = parent[curr];
    }
    for (int i = 0; i < lastPathLen; i++) {
        lastShortestPath[i] = tempPath[lastPathLen - 1 - i];
    }
    lastRouteTarget = dest;
    lastRouteCost = dist[dest];

    if (verbose) {
        printf("\n========== DIJKSTRA SMOKE-FREE FIREFIGHTING ROUTE ==========\n");
        printf("Origin      : %s\n", graphNodeNames[src]);
        printf("Destination : %s\n", graphNodeNames[dest]);
        if (dist[dest] >= INF) {
            printf("STATUS      : ALL CORRIDORS CUT OFF BY PEAT FIRE!\n");
        } else {
            printf("Safe Route  : ");
            for (int i = 0; i < lastPathLen; i++) {
                printf("%s%s", graphNodeNames[lastShortestPath[i]], (i < lastPathLen - 1) ? " -> " : "\n");
            }
            printf("Effective Cost : %d km (Smoke/Fire Hazard-Adjusted)\n", dist[dest]);
        }
    }
}

// ===============================
// ARRAY, SEARCH, SORT, STACK, LISTS
// ===============================
void addEmergencyCore(const char *sectorCode, const char *type, const char *location,
                      double lat, double lon, float ndvi, float ndwi, float thermalK,
                      float severityScore, int priority, int targetNode) {
    if (emergencyCount >= MAX_EMERGENCIES) return;

    Emergency e;
    e.id = nextId++;
    strncpy(e.sectorCode, sectorCode, 15);
    e.sectorCode[15] = '\0';
    strncpy(e.type, type, NAME_LEN - 1);
    e.type[NAME_LEN - 1] = '\0';
    strncpy(e.location, location, LOC_LEN - 1);
    e.location[LOC_LEN - 1] = '\0';
    e.lat = lat;
    e.lon = lon;
    e.ndvi = ndvi;
    e.ndwi = ndwi;
    e.thermalK = thermalK;
    e.severityScore = severityScore;
    e.priority = priority;
    e.targetGraphNode = targetNode;

    emergencies[emergencyCount++] = e;
}

void addEmergency(void) {
    char type[NAME_LEN], location[LOC_LEN];
    int priority;
    printf("Enter hazard type (Peat Fire, Crown Wildfire, Smoke Haze): ");
    clearInputBuffer();
    readLine(type, NAME_LEN);
    printf("Enter Indonesian sector location: ");
    readLine(location, LOC_LEN);
    printf("Enter priority (1-5): ");
    scanf("%d", &priority);
    if (priority < 1) priority = 1;
    if (priority > 5) priority = 5;
    addEmergencyCore("IDN-MAN", type, location, -2.5800, 113.8800, 0.12f, -0.35f, 360.0f, priority * 19.0f, priority, 4);
}

void displayEmergency(void) {
    if (emergencyCount == 0) {
        printf("No emergencies recorded.\n");
        return;
    }
    printf("\n%-5s %-10s %-15s %-28s %-6s %-6s %-7s %-8s %-4s\n",
           "ID", "Sector", "Type", "Location", "NDVI", "NDWI", "Temp(K)", "Severity", "Prio");
    printf("--------------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < emergencyCount; i++) {
        printf("%-5d %-10s %-15s %-28s %-6.2f %-6.2f %-7.1f %-8.1f %-4d\n",
               emergencies[i].id, emergencies[i].sectorCode, emergencies[i].type,
               emergencies[i].location, emergencies[i].ndvi, emergencies[i].ndwi,
               emergencies[i].thermalK, emergencies[i].severityScore, emergencies[i].priority);
    }
}

// Section 2: Quick Sort by Satellite Severity Score (Descending)
int partition(int low, int high) {
    float pivot = emergencies[high].severityScore;
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (emergencies[j].severityScore >= pivot) {
            i++;
            Emergency temp = emergencies[i];
            emergencies[i] = emergencies[j];
            emergencies[j] = temp;
        }
    }
    Emergency temp = emergencies[i + 1];
    emergencies[i + 1] = emergencies[high];
    emergencies[high] = temp;
    return i + 1;
}

void quickSortHelper(int low, int high) {
    if (low < high) {
        int pi = partition(low, high);
        quickSortHelper(low, pi - 1);
        quickSortHelper(pi + 1, high);
    }
}

void quickSort(void) {
    if (emergencyCount > 0) quickSortHelper(0, emergencyCount - 1);
    printf("Hotspots sorted by Spectral Severity Score using Quick Sort O(n log n).\n");
}

// Section 2: Binary Search by ID
void sortById(Emergency temp[], int low, int high) {
    if (low >= high) return;
    int i = low, j = high, pivot = temp[(low + high) / 2].id;
    while (i <= j) {
        while (temp[i].id < pivot) i++;
        while (temp[j].id > pivot) j--;
        if (i <= j) {
            Emergency v = temp[i]; temp[i] = temp[j]; temp[j] = v;
            i++; j--;
        }
    }
    if (low < j) sortById(temp, low, j);
    if (i < high) sortById(temp, i, high);
}

void binarySearch(void) {
    if (emergencyCount == 0) { printf("No emergencies.\n"); return; }
    int id;
    printf("Enter emergency ID to search: ");
    scanf("%d", &id);
    Emergency temp[MAX_EMERGENCIES];
    for (int i = 0; i < emergencyCount; i++) temp[i] = emergencies[i];
    sortById(temp, 0, emergencyCount - 1);

    int low = 0, high = emergencyCount - 1, found = -1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (temp[mid].id == id) { found = mid; break; }
        if (temp[mid].id < id) low = mid + 1;
        else high = mid - 1;
    }
    if (found == -1) printf("Emergency %d not found.\n", id);
    else printf("Found -> ID: %d | %s at %s | Severity: %.1f (Priority %d)\n",
                temp[found].id, temp[found].type, temp[found].location,
                temp[found].severityScore, temp[found].priority);
}

// Section 3: Stack & Linked Lists
void pushCore(int id) {
    if (stackTop < MAX_STACK - 1) historyStack[++stackTop] = id;
}

void insertSinglyCore(const char *resource) {
    SNode *node = (SNode *)malloc(sizeof(SNode));
    strncpy(node->resource, resource, NAME_LEN - 1);
    node->next = NULL;
    if (!singlyHead) singlyHead = singlyTail = node;
    else { singlyTail->next = node; singlyTail = node; }
}

void insertCircularSat(const char *satName, const char *sensor) {
    CNode *node = (CNode *)malloc(sizeof(CNode));
    strncpy(node->satName, satName, NAME_LEN - 1);
    strncpy(node->sensorType, sensor, NAME_LEN - 1);
    if (!circularHead) {
        circularHead = node;
        node->next = node;
        activeSatellite = node;
    } else {
        CNode *curr = circularHead;
        while (curr->next != circularHead) curr = curr->next;
        curr->next = node;
        node->next = circularHead;
    }
}

void rotateSatelliteOrbit(void) {
    if (activeSatellite) {
        activeSatellite = activeSatellite->next;
        printf("Orbital Handover -> Active Satellite is now [%s (%s)]\n",
               activeSatellite->satName, activeSatellite->sensorType);
    }
}

// ===============================
// INDONESIA PEAT & FOREST FIRE TELEMETRY ENGINE
// ===============================
void evaluateSectorTelemetry(SectorCell *cell) {
    float score = 0.0f;
    char hazardType[NAME_LEN] = "Normal";
    float deltaNdvi = cell->ndviBaseline - cell->ndvi;

    // 1. Extreme Crown Wildfire / National Park Blaze (Thermal > 350K)
    if (cell->thermalK >= 350.0f && cell->ndwi > -0.25f) {
        score = 65.0f + (cell->thermalK - 350.0f) * 1.1f;
        strcpy(hazardType, "Crown Wildfire");
    }
    // 2. Peatland Sub-Surface Fire (Desiccated Peat NDWI <= -0.25 + High Thermal)
    else if (cell->ndwi <= -0.25f && cell->thermalK >= 332.0f) {
        score = 58.0f + (fabsf(cell->ndwi) * 55.0f) + (cell->thermalK - 330.0f) * 0.7f;
        strcpy(hazardType, "Peatland Fire");
    }
    // 3. Severe Burn Scar & Transboundary Haze Source (Delta NDVI >= 0.30)
    else if (deltaNdvi >= 0.30f) {
        score = 45.0f + (deltaNdvi * 85.0f);
        strcpy(hazardType, "Smoke & Burn Scar");
    } else {
        score = 12.0f;
    }

    if (score > 99.8f) score = 99.8f;
    cell->severityScore = score;

    insertHashTable(*cell);
    if (score >= 35.0f) {
        bstRoot = insertBST(bstRoot, *cell);
    }

    if (score >= 45.0f) {
        int prio = (score >= 85.0f) ? 5 : (score >= 72.0f) ? 4 : (score >= 55.0f) ? 3 : 2;
        addEmergencyCore(cell->sectorCode, hazardType, cell->areaName,
                         cell->lat, cell->lon, cell->ndvi, cell->ndwi,
                         cell->thermalK, score, prio, cell->mappedGraphNode);

        // If intense peat fire / zero-visibility smoke hits transit bridges (Node 1 or Node 2), penalize graph roads
        if (cell->mappedGraphNode > 0 && score >= 68.0f) {
            int gNode = cell->mappedGraphNode;
            nodeBlocked[gNode] = 1;
            if (gNode == 1 || gNode == 2) {
                for (int k = 0; k < NUM_GRAPH_NODES; k++) {
                    if (liveAdjMatrix[gNode][k] != INF && liveAdjMatrix[gNode][k] > 0) {
                        liveAdjMatrix[gNode][k] += 120; // Smoke & Peat Collapse Detour Penalty
                        liveAdjMatrix[k][gNode] += 120;
                    }
                }
            }
        }
    }
}

void ingestSatelliteCSV(const char *filename) {
    FILE *fp = fopen(filename, "r");
    emergencyCount = 0;
    nextId = 1001;
    freeBST(bstRoot);
    bstRoot = NULL;
    initHashTable();
    initKalimantanGraph();

    if (fp != NULL) {
        char line[256];
        fgets(line, sizeof(line), fp);
        while (fgets(line, sizeof(line), fp)) {
            SectorCell c;
            if (sscanf(line, "%d,%d,%15[^,],%99[^,],%lf,%lf,%f,%f,%f,%f,%d",
                       &c.row, &c.col, c.sectorCode, c.areaName,
                       &c.lat, &c.lon, &c.ndvi, &c.ndviBaseline,
                       &c.ndwi, &c.thermalK, &c.mappedGraphNode) == 11) {
                evaluateSectorTelemetry(&c);
                if (c.row >= 0 && c.row < GRID_ROWS && c.col >= 0 && c.col < GRID_COLS) {
                    sectorGrid[c.row][c.col] = c;
                }
            }
        }
        fclose(fp);
        printf("[SATELLITE INGESTION] Loaded Indonesia Oct 2026 Hotspots from '%s'.\n", filename);
    } else {
        printf("[SATELLITE INGESTION] Loading October 2026 Indonesia 799-Hotspot Baseline...\n");
        SectorCell sampleGrid[7] = {
            {0, 1, "IDN-KLM1", "Sebangau_Peat_Kalimantan", -2.5800, 113.8800, 0.14f, 0.72f, -0.44f, 369.5f, 4},
            {0, 2, "IDN-JVA1", "Bromo_Tengger_Semeru_NP",  -7.9425, 112.9530, 0.11f, 0.65f, -0.18f, 378.2f, 4},
            {1, 1, "IDN-KLM2", "Tumbang_Nusa_Peat_Bridge", -2.3850, 114.0920, 0.18f, 0.68f, -0.39f, 358.4f, 1},
            {1, 2, "IDN-SUM1", "Ogan_Komering_Ilir_Sumatra",-3.3800, 105.1500, 0.16f, 0.70f, -0.36f, 354.0f, 2},
            {2, 1, "IDN-KLM3", "Pulang_Pisau_Peatlands",   -2.7540, 114.2600, 0.21f, 0.69f, -0.33f, 349.8f, 2},
            {2, 2, "IDN-PAP1", "Merauke_Savanna_Papua",    -8.4932, 140.4018, 0.22f, 0.64f, -0.22f, 344.5f, 7},
            {3, 1, "IDN-KLM4", "Riau_Malacca_Haze_Corridor",0.5071, 101.4478, 0.25f, 0.66f, -0.20f, 331.0f, 5}
        };
        for (int i = 0; i < 7; i++) {
            evaluateSectorTelemetry(&sampleGrid[i]);
            sectorGrid[sampleGrid[i].row][sampleGrid[i].col] = sampleGrid[i];
        }
    }

    quickSort();
    if (emergencyCount > 0) {
        runDijkstra(0, emergencies[0].targetGraphNode, 0);
    }
}

// ===============================
// EXPORT JSON FOR WEB FRONTEND
// ===============================
void exportStateJSON(const char *outFile) {
    FILE *fp = fopen(outFile, "w");
    if (!fp) return;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"activeSatellite\": \"%s (%s)\",\n",
            activeSatellite ? activeSatellite->satName : "Suomi-NPP VIIRS",
            activeSatellite ? activeSatellite->sensorType : "375m Thermal Hotspot");

    fprintf(fp, "  \"emergencies\": [\n");
    for (int i = 0; i < emergencyCount; i++) {
        fprintf(fp, "    {\"id\": %d, \"sector\": \"%s\", \"type\": \"%s\", \"location\": \"%s\", \"lat\": %.4f, \"lon\": %.4f, \"ndvi\": %.2f, \"ndwi\": %.2f, \"thermalK\": %.1f, \"severity\": %.1f, \"priority\": %d, \"graphNode\": %d}%s\n",
                emergencies[i].id, emergencies[i].sectorCode, emergencies[i].type,
                emergencies[i].location, emergencies[i].lat, emergencies[i].lon,
                emergencies[i].ndvi, emergencies[i].ndwi, emergencies[i].thermalK,
                emergencies[i].severityScore, emergencies[i].priority,
                emergencies[i].targetGraphNode, (i < emergencyCount - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"graphNodes\": [\n");
    for (int i = 0; i < NUM_GRAPH_NODES; i++) {
        fprintf(fp, "    {\"id\": %d, \"name\": \"%s\", \"lat\": %.4f, \"lon\": %.4f, \"blocked\": %d}%s\n",
                i, graphNodeNames[i], graphNodeCoords[i][0], graphNodeCoords[i][1],
                nodeBlocked[i], (i < NUM_GRAPH_NODES - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"shortestPath\": [");
    for (int i = 0; i < lastPathLen; i++) {
        fprintf(fp, "%d%s", lastShortestPath[i], (i < lastPathLen - 1) ? ", " : "");
    }
    fprintf(fp, "],\n");
    fprintf(fp, "  \"routeCostKm\": %d\n", lastRouteCost);
    fprintf(fp, "}\n");
    fclose(fp);
    printf("[FRONTEND BRIDGE] Exported Indonesia Crisis State to '%s'.\n", outFile);
}

void initSystemDefaults(void) {
    insertCircularSat("Suomi-NPP-VIIRS", "NASA 375m Thermal Hotspot");
    insertCircularSat("Sentinel-2B", "ESA Multispectral NBR/NDVI");
    insertCircularSat("Terra-MODIS", " Thermal & Aerosol Smoke Index");
    insertCircularSat("Himawari-9", "JMA Geostationary Smoke Plume");

    insertSinglyCore("BNPB Water-Bombing Heli Mi-8");
    insertSinglyCore("Manggala Agni Peat Strike Team");
    insertSinglyCore("Kalimantan Deep-Well Pump Rig");
    insertSinglyCore("TNI Medical Evacuation Squad");
}

int main(int argc, char *argv[]) {
    initSystemDefaults();
    initKalimantanGraph();
    initHashTable();

    if (argc > 1 && strcmp(argv[1], "--auto") == 0) {
        ingestSatelliteCSV("telemetry.csv");
        exportStateJSON("state.json");
        return 0;
    }

    int choice;
    ingestSatelliteCSV("telemetry.csv");
    exportStateJSON("state.json");

    do {
        printf("\n========== RESQNET INDONESIA OCT 2026 WILDFIRE DSA ENGINE ==========\n");
        printf(" Active Overhead Satellite : %s\n", activeSatellite ? activeSatellite->satName : "None");
        printf("--------------------------------------------------------------------\n");
        printf(" 1. Ingest Satellite Hotspot Telemetry CSV & Update Structures\n");
        printf(" 2. Display Active Hotspot Zones \n");
        printf(" 3. Quick Sort Hotspots by Spectral Severity \n");
        printf(" 4. Binary Search Hotspot by ID \n");
        printf(" 5. Rotate Satellite Constellation Orbit\n");
        printf(" 6. Display BST of Hotspot Anomalies In-Order\n");
        printf(" 7. Compute Dijkstra Smoke-Safe Route in Kalimantan\n");
        printf(" 8. Lookup Hotspot in Hash Table\n");
        printf(" 9. Display Full Hash Table State\n");
        printf("10. Add Manual Hotspot Override\n");
        printf("11. Export JSON State for Mission Control Web UI\n");
        printf(" 0. Exit\n");
        printf("====================================================================\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) {
            clearInputBuffer();
            continue;
        }

        switch (choice) {
            case 1: ingestSatelliteCSV("telemetry.csv"); exportStateJSON("state.json"); break;
            case 2: displayEmergency(); break;
            case 3: quickSort(); displayEmergency(); break;
            case 4: binarySearch(); break;
            case 5: rotateSatelliteOrbit(); exportStateJSON("state.json"); break;
            case 6:
                printf("\n========== BST IN-ORDER TRAVERSAL (BY SEVERITY) ==========\n");
                inorderBST(bstRoot);
                break;
            case 7:
                if (emergencyCount > 0) runDijkstra(0, emergencies[0].targetGraphNode, 1);
                else runDijkstra(0, 4, 1);
                break;
            case 8: searchHashTable(); break;
            case 9: displayHashTable(); break;
            case 10: addEmergency(); exportStateJSON("state.json"); break;
            case 11: exportStateJSON("state.json"); break;
            case 0: printf("Shutting down RESQNET Ground Station.\n"); break;
            default: printf("Invalid option.\n");
        }
    } while (choice != 0);

    freeBST(bstRoot);
    return 0;
}