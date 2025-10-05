#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// --- Constantes de la Simulación ---
#define NUM_VEHICULOS_TOTAL 50 // Para una prueba corta, luego se puede escalar
#define TIPO_CARRO 0
#define TIPO_CAMION 1
#define SENTIDO_1_A_4 0
#define SENTIDO_4_A_1 1

// --- Semáforos para cada subtramo ---
sem_t sem_tramo1;
sem_t sem_tramo2;
sem_t sem_tramo3;
sem_t sem_tramo4;

// --- Mutex para proteger las estadísticas ---
pthread_mutex_t mutex_estadisticas;

// --- Estructura para las estadísticas (simplificada) ---
// Se puede expandir para cumplir todos los requisitos del problema
typedef struct {
    int vehiculos_tramo1_sentido1_4;
    int vehiculos_tramo1_sentido4_1;
    // ... agregar contadores para todos los tramos y sentidos
    int max_vehiculos_espera;
    double max_tiempo_espera;
} Estadisticas;

Estadisticas stats = {0};

// --- Argumentos para el hilo de cada vehículo ---
typedef struct {
    int id;
    int tipo;
    int sentido;
} VehiculoArgs;

// --- Función que simula el paso por un tramo ---
void pasar_por_tramo(int id, int tramo, int tiempo_seg) {
    printf("Vehiculo #%d está pasando por el TRAMO %d...\n", id, tramo);
    sleep(tiempo_seg); // Simula el tiempo que tarda en cruzar
}

// --- Lógica del hilo para cada vehículo ---
void* vehiculo_thread(void* args) {
    VehiculoArgs* v_args = (VehiculoArgs*)args;
    int id = v_args->id;
    int tipo = v_args->tipo;
    int sentido = v_args->sentido;
    char* tipo_str = (tipo == TIPO_CARRO) ? "Carro" : "Camión";
    char* sentido_str = (sentido == SENTIDO_1_A_4) ? "1->4" : "4->1";

    printf("-> Ha llegado un nuevo vehiculo: #%d (%s) en sentido %s\n", id, tipo_str, sentido_str);

    if (sentido == SENTIDO_1_A_4) {
        // --- VIAJE SENTIDO 1 -> 4 ---

        // Entrar a Tramo 1 (Capacidad 4)
        sem_wait(&sem_tramo1);
        pasar_por_tramo(id, 1, 2);
        sem_post(&sem_tramo1);
        printf("Vehiculo #%d salió del TRAMO 1.\n", id);

        // Entrar a Tramo 2 (Capacidad 2 carros o 1 camión)
        if (tipo == TIPO_CARRO) {
            sem_wait(&sem_tramo2);
        } else { // Camión
            sem_wait(&sem_tramo2);
            sem_wait(&sem_tramo2);
        }
        pasar_por_tramo(id, 2, 3);
        if (tipo == TIPO_CARRO) {
            sem_post(&sem_tramo2);
        } else { // Camión
            sem_post(&sem_tramo2);
            sem_post(&sem_tramo2);
        }
        printf("Vehiculo #%d salió del TRAMO 2.\n", id);
        
        // Entrar a Tramo 3 (Capacidad 1)
        sem_wait(&sem_tramo3);
        pasar_por_tramo(id, 3, 1);
        sem_post(&sem_tramo3);
        printf("Vehiculo #%d salió del TRAMO 3.\n", id);

        // Entrar a Tramo 4 (Capacidad 3)
        sem_wait(&sem_tramo4);
        pasar_por_tramo(id, 4, 2);
        sem_post(&sem_tramo4);
        printf("Vehiculo #%d salió del TRAMO 4. Viaje completado.\n", id);

    } else { // SENTIDO 4 -> 1
        
        // Entrar a Tramo 4 (Capacidad 3)
        sem_wait(&sem_tramo4);
        pasar_por_tramo(id, 4, 2);
        sem_post(&sem_tramo4);
        printf("Vehiculo #%d salió del TRAMO 4.\n", id);
        
        // Entrar a Tramo 3 (Capacidad 1)
        sem_wait(&sem_tramo3);
        pasar_por_tramo(id, 3, 1);
        sem_post(&sem_tramo3);
        printf("Vehiculo #%d salió del TRAMO 3.\n", id);

        // Entrar a Tramo 2 (Capacidad 2 carros o 1 camión)
        if (tipo == TIPO_CARRO) {
            sem_wait(&sem_tramo2);
        } else { // Camión
            sem_wait(&sem_tramo2);
            sem_wait(&sem_tramo2);
        }
        pasar_por_tramo(id, 2, 3);
        if (tipo == TIPO_CARRO) {
            sem_post(&sem_tramo2);
        } else { // Camión
            sem_post(&sem_tramo2);
            sem_post(&sem_tramo2);
        }
        printf("Vehiculo #%d salió del TRAMO 2.\n", id);
        
        // Entrar a Tramo 1 (Capacidad 4)
        sem_wait(&sem_tramo1);
        pasar_por_tramo(id, 1, 2);
        sem_post(&sem_tramo1);
        printf("Vehiculo #%d salió del TRAMO 1. Viaje completado.\n", id);
    }

    free(v_args);
    pthread_exit(NULL);
}


int main() {
    pthread_t vehiculos[NUM_VEHICULOS_TOTAL];
    srand(time(NULL));

    // --- Inicialización de Semáforos ---
    // sem_init(p_sem, pshared, value)
    // pshared = 0 -> el semáforo se comparte entre hilos del mismo proceso
    sem_init(&sem_tramo1, 0, 4);
    sem_init(&sem_tramo2, 0, 2);
    sem_init(&sem_tramo3, 0, 1);
    sem_init(&sem_tramo4, 0, 3);

    // --- Inicialización del Mutex ---
    pthread_mutex_init(&mutex_estadisticas, NULL);

    printf("--- Iniciando Simulación de Tráfico en Autopista ---\n");

    // --- Creación de hilos (vehículos) ---
    for (int i = 0; i < NUM_VEHICULOS_TOTAL; i++) {
        VehiculoArgs* args = malloc(sizeof(VehiculoArgs));
        args->id = i + 1;
        args->tipo = rand() % 2; // 0 para carro, 1 para camión
        args->sentido = rand() % 2; // 0 para 1->4, 1 para 4->1

        pthread_create(&vehiculos[i], NULL, vehiculo_thread, (void*)args);
        
        sleep(rand() % 2); // Simula la llegada escalonada de vehículos
    }

    // --- Esperar a que todos los hilos terminen ---
    for (int i = 0; i < NUM_VEHICULOS_TOTAL; i++) {
        pthread_join(vehiculos[i], NULL);
    }
    
    printf("\n--- Simulación Finalizada ---\n");

    // --- Destrucción de Semáforos y Mutex ---
    sem_destroy(&sem_tramo1);
    sem_destroy(&sem_tramo2);
    sem_destroy(&sem_tramo3);
    sem_destroy(&sem_tramo4);
    pthread_mutex_destroy(&mutex_estadisticas);

    // Aquí se imprimirían las estadísticas finales
    // printf("Total de vehículos en tramo 1 (sentido 1->4): %d\n", stats.vehiculos_tramo1_sentido1_4);

    return 0;
}
