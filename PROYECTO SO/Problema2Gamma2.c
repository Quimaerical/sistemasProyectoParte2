#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define VEHICULOS_POR_HORA 500
#define HORAS_SIMULACION 24
#define TOTAL_VEHICULOS (VEHICULOS_POR_HORA * HORAS_SIMULACION)
#define SEGUNDOS_POR_HORA_SIMULACION 30
#define TOTAL_SEGUNDOS_SIMULACION (SEGUNDOS_POR_HORA_SIMULACION * HORAS_SIMULACION)

// Estructuras de datos
typedef enum { AUTO, CAMION } vehicleType;
typedef enum { DIR_1A4, DIR_4A1 } Direccion;

typedef struct {
    int id;
    vehicleType tipo;
    Direccion dir;
    time_t horaEntrada;
} Vehiculo;

typedef struct {
    sem_t semaforo;
    int capacidad;
    int vehiculosPresentes;
    int contadorAutos;
    int contadorCamiones;
    pthread_mutex_t mutex;
    pthread_cond_t cond_camion;  // Condición para camiones esperando
    pthread_cond_t cond_auto;    // Condición para autos esperando
} Subtramo;

typedef struct {
    sem_t semaforo;
    int vehiculosEsperando;
    int maxEspera;
    time_t tiempoMaxEspera;
    time_t tiempoTotalEspera;
    int totalVehiculosEsperado;
    pthread_mutex_t mutex;
} Hombrillo;

// Variables globales
Subtramo subtramos[4];
Hombrillo hombrillos[3];

// Estadísticas
int estadisticasHorarias[24][2] = {0};
int estadisticasSubtramos[4][2] = {0};
int totalVehiculosDia = 0;
time_t inicioSimulacion;
pthread_mutex_t statsMutex = PTHREAD_MUTEX_INITIALIZER;

// Inicialización de recursos
void inicializar_recursos() {
    subtramos[0].capacidad = 4;
    subtramos[1].capacidad = 2;
    subtramos[2].capacidad = 1;
    subtramos[3].capacidad = 3;
    
    for (int i = 0; i < 4; i++) {
        sem_init(&subtramos[i].semaforo, 0, subtramos[i].capacidad);
        pthread_mutex_init(&subtramos[i].mutex, NULL);
        pthread_cond_init(&subtramos[i].cond_camion, NULL);
        pthread_cond_init(&subtramos[i].cond_auto, NULL);
        subtramos[i].vehiculosPresentes = 0;
        subtramos[i].contadorAutos = 0;
        subtramos[i].contadorCamiones = 0;
    }
    
    for (int i = 0; i < 3; i++) {
        sem_init(&hombrillos[i].semaforo, 0, 9999);
        pthread_mutex_init(&hombrillos[i].mutex, NULL);
        hombrillos[i].vehiculosEsperando = 0;
        hombrillos[i].maxEspera = 0;
        hombrillos[i].tiempoMaxEspera = 0;
        hombrillos[i].tiempoTotalEspera = 0;
        hombrillos[i].totalVehiculosEsperado = 0;
    }
}

// Función mejorada para entrada atómica al subtramo 2
int entrar_subtramo2_atomicamente(Vehiculo* v) {
    pthread_mutex_lock(&subtramos[1].mutex);
    
    int puede_entrar = 0;
    if (v->tipo == AUTO) {
        // Autos: máximo 2, y solo si no hay camiones
        puede_entrar = (subtramos[1].contadorAutos < 2 && subtramos[1].contadorCamiones == 0);
        if (!puede_entrar) {
            // Esperar hasta que haya espacio para autos
            while (!(subtramos[1].contadorAutos < 2 && subtramos[1].contadorCamiones == 0)) {
                pthread_cond_wait(&subtramos[1].cond_auto, &subtramos[1].mutex);
            }
            puede_entrar = 1;
        }
    } else { // CAMION
        // Camiones: solo 1, y solo si no hay vehículos
        puede_entrar = (subtramos[1].vehiculosPresentes == 0);
        if (!puede_entrar) {
            // Esperar hasta que no haya vehículos
            while (subtramos[1].vehiculosPresentes > 0) {
                pthread_cond_wait(&subtramos[1].cond_camion, &subtramos[1].mutex);
            }
            puede_entrar = 1;
        }
    }
    
    if (puede_entrar) {
        subtramos[1].vehiculosPresentes++;
        if (v->tipo == AUTO) {
            subtramos[1].contadorAutos++;
        } else {
            subtramos[1].contadorCamiones++;
        }
        // Tomar el semáforo para controlar capacidad
        sem_wait(&subtramos[1].semaforo);
    }
    
    pthread_mutex_unlock(&subtramos[1].mutex);
    return puede_entrar;
}

// Función para salir atómicamente del subtramo 2
void salir_subtramo2_atomicamente(Vehiculo* v) {
    pthread_mutex_lock(&subtramos[1].mutex);
    
    subtramos[1].vehiculosPresentes--;
    if (v->tipo == AUTO) {
        subtramos[1].contadorAutos--;
        // Notificar a autos y camiones esperando
        if (subtramos[1].contadorAutos == 0) {
            pthread_cond_broadcast(&subtramos[1].cond_camion);
        }
        pthread_cond_broadcast(&subtramos[1].cond_auto);
    } else {
        subtramos[1].contadorCamiones--;
        // Notificar a autos y camiones esperando
        pthread_cond_broadcast(&subtramos[1].cond_auto);
        pthread_cond_broadcast(&subtramos[1].cond_camion);
    }
    
    sem_post(&subtramos[1].semaforo);
    pthread_mutex_unlock(&subtramos[1].mutex);
}

int obtener_hora_actual() {
    time_t ahora = time(NULL);
    double segundos_transcurridos = difftime(ahora, inicioSimulacion);
    int hora_simulacion = (int)(segundos_transcurridos / SEGUNDOS_POR_HORA_SIMULACION) % 24;
    return hora_simulacion;
}

void actualizar_estadisticas_horarias(Direccion dir) {
    int hora = obtener_hora_actual();
    if (hora >= 0 && hora < 24) {
        pthread_mutex_lock(&statsMutex);
        if (dir == DIR_1A4) {
            estadisticasHorarias[hora][0]++;
        } else {
            estadisticasHorarias[hora][1]++;
        }
        totalVehiculosDia++;
        pthread_mutex_unlock(&statsMutex);
    }
}

// Función mejorada del vehículo
// Función corregida y simplificada del vehículo
void* vehiculoThread(void* arg) {
    Vehiculo* v = (Vehiculo*)arg;
    int inicio, fin, paso;
    
    if (v->dir == DIR_1A4) {
        inicio = 0; fin = 3; paso = 1;
        printf("🟢 Vehículo %d (%s) INICIANDO viaje dirección 1→4\n", 
               v->id, (v->tipo == AUTO) ? "Auto" : "Camión");
    } else {
        inicio = 3; fin = 0; paso = -1;
        printf("🔵 Vehículo %d (%s) INICIANDO viaje dirección 4→1\n", 
               v->id, (v->tipo == AUTO) ? "Auto" : "Camión");
    }
    
    actualizar_estadisticas_horarias(v->dir);
    
    // Entrar al primer subtramo
    printf("➡️  Vehículo %d entrando al subtramo %d\n", v->id, inicio + 1);
    
    if (inicio == 1) {
        entrar_subtramo2_atomicamente(v);
    } else {
        sem_wait(&subtramos[inicio].semaforo);
        pthread_mutex_lock(&subtramos[inicio].mutex);
        subtramos[inicio].vehiculosPresentes++;
        if (v->tipo == AUTO) {
            subtramos[inicio].contadorAutos++;
        } else {
            subtramos[inicio].contadorCamiones++;
        }
        pthread_mutex_unlock(&subtramos[inicio].mutex);
    }
    
    estadisticasSubtramos[inicio][v->dir]++;
    
    // Recorrer todos los subtramos
    for (int i = inicio; i != fin + paso; i += paso) {
        int siguiente = i + paso;
        
        if (siguiente == fin + paso) {
            // Último subtramo - salir y terminar
            printf("🎉 Vehículo %d COMPLETÓ su viaje en subtramo %d\n", v->id, i + 1);
            
            if (i == 1) {
                salir_subtramo2_atomicamente(v);
            } else {
                pthread_mutex_lock(&subtramos[i].mutex);
                subtramos[i].vehiculosPresentes--;
                if (v->tipo == AUTO) {
                    subtramos[i].contadorAutos--;
                } else {
                    subtramos[i].contadorCamiones--;
                }
                pthread_mutex_unlock(&subtramos[i].mutex);
                sem_post(&subtramos[i].semaforo);
            }
            break;
        }
        
        // Simular tiempo en el subtramo actual
        int tiempo_subtramo = (rand() % 2) + 1;
        printf("🚗 Vehículo %d CIRCULANDO en subtramo %d (%d segundos)\n", 
               v->id, i + 1, tiempo_subtramo);
        usleep(tiempo_subtramo * 35000);
        
        // Salir del subtramo actual
        if (i == 1) {
            salir_subtramo2_atomicamente(v);
        } else {
            pthread_mutex_lock(&subtramos[i].mutex);
            subtramos[i].vehiculosPresentes--;
            if (v->tipo == AUTO) {
                subtramos[i].contadorAutos--;
            } else {
                subtramos[i].contadorCamiones--;
            }
            pthread_mutex_unlock(&subtramos[i].mutex);
            sem_post(&subtramos[i].semaforo);
        }
        
        printf("✅ Vehículo %d SALIÓ del subtramo %d\n", v->id, i + 1);
        
        // Calcular índice del hombrillo
        int hombrillo_idx;
        if (paso > 0) {
            hombrillo_idx = i;
        } else {
            hombrillo_idx = i - 1;
        }
        
        // *** ENTRADA AL SIGUIENTE SUBTRAMO - VERSIÓN SIMPLIFICADA ***
        time_t inicio_espera = time(NULL);
        int en_hombrillo = 0;
        
        // Intentar entrar al siguiente subtramo
        if (siguiente == 1) {
            // Para subtramo 2 - usar función atómica
            if (!entrar_subtramo2_atomicamente(v)) {
                // No pudo entrar inmediatamente - ir al hombrillo
                printf("🟡 Vehículo %d → Subtramo 2 LLENO, YENDO al hombrillo %d\n", 
                       v->id, hombrillo_idx + 1);
                en_hombrillo = 1;
                
                // Entrar al hombrillo
                pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
                hombrillos[hombrillo_idx].vehiculosEsperando++;
                if (hombrillos[hombrillo_idx].vehiculosEsperando > hombrillos[hombrillo_idx].maxEspera) {
                    hombrillos[hombrillo_idx].maxEspera = hombrillos[hombrillo_idx].vehiculosEsperando;
                }
                pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
                
                // Esperar en el hombrillo hasta que pueda entrar
                while (!entrar_subtramo2_atomicamente(v)) {
                    usleep(50000); // Esperar 0.05 segundos antes de intentar nuevamente
                }
                
                // Salir del hombrillo
                pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
                hombrillos[hombrillo_idx].vehiculosEsperando--;
                pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
            }
        } else {
            // Para otros subtramos - intentar con sem_wait (que es atómico)
            if (sem_trywait(&subtramos[siguiente].semaforo) != 0) {
                // No pudo entrar inmediatamente - ir al hombrillo
                printf("🟡 Vehículo %d → Subtramo %d LLENO, YENDO al hombrillo %d\n", 
                       v->id, siguiente + 1, hombrillo_idx + 1);
                en_hombrillo = 1;
                
                // Entrar al hombrillo
                pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
                hombrillos[hombrillo_idx].vehiculosEsperando++;
                if (hombrillos[hombrillo_idx].vehiculosEsperando > hombrillos[hombrillo_idx].maxEspera) {
                    hombrillos[hombrillo_idx].maxEspera = hombrillos[hombrillo_idx].vehiculosEsperando;
                }
                pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
                
                // Esperar en el hombrillo con sem_wait normal (bloqueante)
                sem_wait(&subtramos[siguiente].semaforo);
                
                // Salir del hombrillo
                pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
                hombrillos[hombrillo_idx].vehiculosEsperando--;
                pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
            }
            
            // Actualizar contadores del subtramo (para subtramos normales)
            pthread_mutex_lock(&subtramos[siguiente].mutex);
            subtramos[siguiente].vehiculosPresentes++;
            if (v->tipo == AUTO) {
                subtramos[siguiente].contadorAutos++;
            } else {
                subtramos[siguiente].contadorCamiones++;
            }
            pthread_mutex_unlock(&subtramos[siguiente].mutex);
        }
        
        // Si estuvo en el hombrillo, calcular tiempo de espera
        if (en_hombrillo) {
            time_t fin_espera = time(NULL);
            time_t duracion_espera = fin_espera - inicio_espera;
            
            // Actualizar estadísticas de espera
            pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
            if (duracion_espera > hombrillos[hombrillo_idx].tiempoMaxEspera) {
                hombrillos[hombrillo_idx].tiempoMaxEspera = duracion_espera;
            }
            hombrillos[hombrillo_idx].tiempoTotalEspera += duracion_espera;
            hombrillos[hombrillo_idx].totalVehiculosEsperado++;
            pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
            
            printf("⏱️  Vehículo %d ESPERÓ %ld segundos en hombrillo %d\n", 
                   v->id, duracion_espera, hombrillo_idx + 1);
        }
        
        // Actualizar estadísticas del siguiente subtramo
        estadisticasSubtramos[siguiente][v->dir]++;
        printf("➡️  Vehículo %d ENTRÓ al subtramo %d\n", v->id, siguiente + 1);
    }
    
    printf("🏁 Vehículo %d terminó su recorrido\n", v->id);
    free(v);
    return NULL;
}

void mostrar_estadisticas() {
    printf("\n📊 ========== ESTADÍSTICAS FINALES ==========\n");
    
    printf("\n📈 ESTADÍSTICAS HORARIAS (vehículos generados por hora):\n");
    printf("Hora | Dirección 1→4 | Dirección 4→1 | Total\n");
    printf("-----|---------------|---------------|-------\n");
    for (int hora = 0; hora < 24; hora++) {
        int total_hora = estadisticasHorarias[hora][0] + estadisticasHorarias[hora][1];
        printf("%2d   | %13d | %13d | %5d\n", 
               hora+1, estadisticasHorarias[hora][0], estadisticasHorarias[hora][1], total_hora);
    }
    
    printf("\n🛣️  ESTADÍSTICAS POR SUBTRAMO (vehículos que circularon):\n");
    for (int i = 0; i < 4; i++) {
        int total_subtramo = estadisticasSubtramos[i][0] + estadisticasSubtramos[i][1];
        printf("Subtramo %d:\n", i + 1);
        printf("  Dirección 1→4: %d vehículos\n", estadisticasSubtramos[i][0]);
        printf("  Dirección 4→1: %d vehículos\n", estadisticasSubtramos[i][1]);
        printf("  Total: %d vehículos\n", total_subtramo);
    }
    
    printf("\n🅿️  ESTADÍSTICAS DE HOMBRILLOS:\n");
    for (int i = 0; i < 3; i++) {
        printf("Hombrillo %d-%d:\n", i + 1, i + 2);
        printf("  Máximo vehículos esperando: %d\n", hombrillos[i].maxEspera);
        printf("  Tiempo máximo de espera: %ld segundos\n", hombrillos[i].tiempoMaxEspera);
        if (hombrillos[i].totalVehiculosEsperado > 0) {
            double promedio = (double)hombrillos[i].tiempoTotalEspera / hombrillos[i].totalVehiculosEsperado;
            printf("  Tiempo promedio de espera: %.2f segundos\n", promedio);
        }
        printf("  Total vehículos que esperaron: %d\n", hombrillos[i].totalVehiculosEsperado);
    }
    
    printf("\n📦 TOTAL DE VEHÍCULOS EN EL DÍA: %d\n", totalVehiculosDia);
    printf("==========================================\n");
}

void limpiar_recursos() {
    for (int i = 0; i < 4; i++) {
        sem_destroy(&subtramos[i].semaforo);
        pthread_mutex_destroy(&subtramos[i].mutex);
        pthread_cond_destroy(&subtramos[i].cond_auto);
        pthread_cond_destroy(&subtramos[i].cond_camion);
    }
    
    for (int i = 0; i < 3; i++) {
        sem_destroy(&hombrillos[i].semaforo);
        pthread_mutex_destroy(&hombrillos[i].mutex);
    }
    
    pthread_mutex_destroy(&statsMutex);
}

int main() {
    srand(time(NULL));
    inicioSimulacion = time(NULL);
    inicializar_recursos();

    printf("🚦 INICIANDO SIMULACIÓN DE TRÁFICO MEJORADA\n");
    printf("⏰ Duración real: %d segundos\n", TOTAL_SEGUNDOS_SIMULACION);
    printf("⏰ Duración simulada: %d horas\n", HORAS_SIMULACION);
    printf("🚗 Vehículos por hora: %d\n", VEHICULOS_POR_HORA);
    printf("📊 Total de vehículos: %d\n", TOTAL_VEHICULOS);
    printf("==========================================\n");

    int vehiculosGenerados = 0;
    
    while (vehiculosGenerados < TOTAL_VEHICULOS) {
        if (difftime(time(NULL), inicioSimulacion) >= TOTAL_SEGUNDOS_SIMULACION) {
            printf("⏰ TIEMPO DE SIMULACIÓN COMPLETADO\n");
            break;
        }
        
        Vehiculo* v = malloc(sizeof(Vehiculo));
        v->id = vehiculosGenerados + 1;
        v->tipo = (rand() % 4 == 0) ? CAMION : AUTO;
        v->dir = (rand() % 2) ? DIR_1A4 : DIR_4A1;
        v->horaEntrada = time(NULL);
        
        pthread_t hilo;
        pthread_create(&hilo, NULL, vehiculoThread, v);
        pthread_detach(hilo);
        
        vehiculosGenerados++;
        usleep(55000 + (rand() % 10001));
    }
    
    printf("✅ GENERACIÓN DE VEHÍCULOS COMPLETADA\n");
    printf("⏳ Esperando que terminen los vehículos en circulación...\n");
    
    sleep(10);
    
    mostrar_estadisticas();
    limpiar_recursos();
    
    printf("🎯 SIMULACIÓN COMPLETADA EXITOSAMENTE\n");
    printf("⏱️  Tiempo real de ejecución: %.0f segundos\n", difftime(time(NULL), inicioSimulacion));
    return 0;
}