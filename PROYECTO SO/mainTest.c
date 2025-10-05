#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define NUM_SUBTRAMOS 4
#define MAX_VEHICULOS 2000
#define HORAS_SIMULACION 24

// Estructuras de datos
typedef enum { AUTO, CAMION } TipoVehiculo;
typedef enum { DIR_1A4, DIR_4A1 } Direccion;

typedef struct {
    int id;
    TipoVehiculo tipo;
    Direccion direccion;
    time_t hora_entrada;
} Vehiculo;

typedef struct {
    sem_t semaforo;
    int capacidad;
    int vehiculos_presentes;
    int contador_autos;
    int contador_camiones;
    pthread_mutex_t mutex;
} Subtramo;

typedef struct {
    sem_t semaforo;
    int vehiculos_esperando;
    int max_espera;
    time_t tiempo_max_espera;
    pthread_mutex_t mutex;
} Hombrillo;

// Variables globales
Subtramo subtramos[NUM_SUBTRAMOS];
Hombrillo hombrillos[NUM_SUBTRAMOS - 1];
int estadisticas[NUM_SUBTRAMOS][2] = {0}; // [subtramo][direccion]
int total_vehiculos_dia = 0;

// Inicialización de recursos
void inicializar_recursos() {
    // Configurar capacidades de subtramos
    subtramos[0].capacidad = 4;  // Subtramo 1
    subtramos[1].capacidad = 2;  // Subtramo 2 (equivalente en autos)
    subtramos[2].capacidad = 1;  // Subtramo 3
    subtramos[3].capacidad = 3;  // Subtramo 4
    
    for (int i = 0; i < NUM_SUBTRAMOS; i++) {
        sem_init(&subtramos[i].semaforo, 0, subtramos[i].capacidad);
        pthread_mutex_init(&subtramos[i].mutex, NULL);
        subtramos[i].vehiculos_presentes = 0;
        subtramos[i].contador_autos = 0;
        subtramos[i].contador_camiones = 0;
    }
    
    for (int i = 0; i < NUM_SUBTRAMOS - 1; i++) {
        sem_init(&hombrillos[i].semaforo, 0, 5); // Capacidad del hombrillo
        pthread_mutex_init(&hombrillos[i].mutex, NULL);
        hombrillos[i].vehiculos_esperando = 0;
        hombrillos[i].max_espera = 0;
    }
}

// Función para verificar si un vehículo puede entrar al subtramo 2
int puede_entrar_subtramo2(TipoVehiculo tipo) {
    pthread_mutex_lock(&subtramos[1].mutex);
    
    int puede_entrar = 0;
    if (tipo == AUTO) {
        puede_entrar = (subtramos[1].contador_autos < 2);
    } else { // CAMION
        puede_entrar = (subtramos[1].contador_camiones == 0 && 
                       subtramos[1].contador_autos == 0);
    }
    
    pthread_mutex_unlock(&subtramos[1].mutex);
    return puede_entrar;
}

// Función principal del vehículo
void* vehiculo_thread(void* arg) {
    Vehiculo* v = (Vehiculo*)arg;
    int inicio, fin, paso;
    
    // Determinar dirección del viaje
    if (v->direccion == DIR_1A4) {
        inicio = 0; fin = 3; paso = 1;
    } else {
        inicio = 3; fin = 0; paso = -1;
    }
    
    printf("Vehículo %d (%s) iniciando viaje dirección %s\n", 
           v->id, 
           (v->tipo == AUTO) ? "Auto" : "Camión",
           (v->direccion == DIR_1A4) ? "1->4" : "4->1");
    
    for (int i = inicio; i != fin + paso; i += paso) {
        // Esperar en hombrillo si es necesario (excepto en el primer subtramo)
        if (i != inicio) {
            int hombrillo_idx = (paso > 0) ? i - 1 : i;
            
            pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
            hombrillos[hombrillo_idx].vehiculos_esperando++;
            if (hombrillos[hombrillo_idx].vehiculos_esperando > hombrillos[hombrillo_idx].max_espera) {
                hombrillos[hombrillo_idx].max_espera = hombrillos[hombrillo_idx].vehiculos_esperando;
            }
            pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
            
            sem_wait(&hombrillos[hombrillo_idx].semaforo);
        }
        
        // Esperar para entrar al subtramo (condición especial para subtramo 2)
        if (i == 1) { // Subtramo 2
            while (!puede_entrar_subtramo2(v->tipo)) {
                usleep(100000); // Espera activa breve (debería mejorarse con condition variables)
            }
        }
        
        sem_wait(&subtramos[i].semaforo);
        
        // Actualizar contadores del subtramo
        pthread_mutex_lock(&subtramos[i].mutex);
        subtramos[i].vehiculos_presentes++;
        if (v->tipo == AUTO) {
            subtramos[i].contador_autos++;
        } else {
            subtramos[i].contador_camiones++;
        }
        pthread_mutex_unlock(&subtramos[i].mutex);
        
        // Simular tiempo en el subtramo
        printf("Vehículo %d en subtramo %d\n", v->id, i + 1);
        sleep(rand() % 3 + 1); // Tiempo aleatorio entre 1-3 segundos
        
        // Liberar subtramo
        pthread_mutex_lock(&subtramos[i].mutex);
        subtramos[i].vehiculos_presentes--;
        if (v->tipo == AUTO) {
            subtramos[i].contador_autos--;
        } else {
            subtramos[i].contador_camiones--;
        }
        pthread_mutex_unlock(&subtramos[i].mutex);
        
        sem_post(&subtramos[i].semaforo);
        
        // Liberar hombrillo
        if (i != fin) {
            int hombrillo_idx = (paso > 0) ? i : i - 1;
            sem_post(&hombrillos[hombrillo_idx].semaforo);
            
            pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
            hombrillos[hombrillo_idx].vehiculos_esperando--;
            pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
        }
        
        // Actualizar estadísticas
        pthread_mutex_lock(&subtramos[i].mutex);
        estadisticas[i][v->direccion]++;
        pthread_mutex_unlock(&subtramos[i].mutex);
    }
    
    printf("Vehículo %d completó su viaje\n", v->id);
    free(v);
    return NULL;
}

// Función para generar vehículos
void* generador_vehiculos(void* arg) {
    int vehiculos_generados = 0;
    
    while (vehiculos_generados < MAX_VEHICULOS * HORAS_SIMULACION) {
        Vehiculo* v = malloc(sizeof(Vehiculo));
        v->id = vehiculos_generados + 1;
        v->tipo = (rand() % 4 == 0) ? CAMION : AUTO; // 25% camiones, 75% autos
        v->direccion = (rand() % 2) ? DIR_1A4 : DIR_4A1;
        v->hora_entrada = time(NULL);
        
        pthread_t hilo;
        pthread_create(&hilo, NULL, vehiculo_thread, v);
        pthread_detach(hilo);
        
        vehiculos_generados++;
        total_vehiculos_dia++;
        
        // Esperar tiempo aleatorio entre generación de vehículos
        //usleep(rand() % 1000000); // Entre 0-1 segundo
        //usleep(rand() % 100000);
    }
    
    return NULL;
}

// Función para mostrar estadísticas
void mostrar_estadisticas() {
    printf("\n=== ESTADÍSTICAS FINALES ===\n");
    for (int i = 0; i < NUM_SUBTRAMOS; i++) {
        printf("Subtramo %d:\n", i + 1);
        printf("  Dirección 1->4: %d vehículos\n", estadisticas[i][DIR_1A4]);
        printf("  Dirección 4->1: %d vehículos\n", estadisticas[i][DIR_4A1]);
        printf("  Total: %d vehículos\n", estadisticas[i][DIR_1A4] + estadisticas[i][DIR_4A1]);
    }
    
    printf("\nMáxima espera en hombrillos:\n");
    for (int i = 0; i < NUM_SUBTRAMOS - 1; i++) {
        printf("Hombrillo %d-%d: %d vehículos\n", i + 1, i + 2, hombrillos[i].max_espera);
    }
    
    printf("\nTotal de vehículos en el día: %d\n", total_vehiculos_dia);
}

int main() {
    srand(time(NULL));
    inicializar_recursos();
    
    printf("Iniciando simulación de tráfico...\n");
    
    pthread_t generador;
    pthread_create(&generador, NULL, generador_vehiculos, NULL);
    
    // Esperar a que termine la simulación (en realidad debería tener una condición de parada)
    sleep(HORAS_SIMULACION * 2); // Simulación acelerada
    
    mostrar_estadisticas();
    
    // Limpiar recursos
    for (int i = 0; i < NUM_SUBTRAMOS; i++) {
        sem_destroy(&subtramos[i].semaforo);
        pthread_mutex_destroy(&subtramos[i].mutex);
    }
    
    for (int i = 0; i < NUM_SUBTRAMOS - 1; i++) {
        sem_destroy(&hombrillos[i].semaforo);
        pthread_mutex_destroy(&hombrillos[i].mutex);
    }
    
    return 0;
}