#include "sensor_mpu.h"
#include <Wire.h>
#include "config.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <MPU6050.h>

extern MPU6050 mpu;
extern uint8_t buffer_dados[];
extern size_t buffer_index;
extern SemaphoreHandle_t mutex_buffer;

int inicializar_mpu(void) {
  Serial.println("[MPU] Inicializando...");

  // Inicializa registradores padrão
  mpu.initialize();
  vTaskDelay(pdMS_TO_TICKS(100));

  if (!mpu.testConnection()) {
    Serial.println("[ERRO] Conexão com MPU6050 falhou.");
    return 0;
  }

  // WHO_AM_I extra
  // uint8_t whoami = mpu.getDeviceID();  // WHO_AM_I = 0x68 esperado
  // if (whoami != 0x68) {
  //   Serial.print("[ERRO] MPU WHO_AM_I inválido: 0x");
  //   Serial.println(whoami, HEX);
  //   return 0;
  // }

  // Configurações básicas (registradores de forma direta via I2Cdevlib)
  mpu.setSleepEnabled(false);                           // Desliga modo de sono
  mpu.setClockSource(MPU6050_CLOCK_PLL_XGYRO);          // Usa clock do giroscópio X
  mpu.setRate(4);                                       // 1 kHz / (1 + 4) = 200 Hz
  mpu.setDLPFMode(3);                                   // 44 Hz bandwidth
  mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);       // ±250 °/s
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);       // ±2g

  mpu.setFIFOEnabled(false); // Desativa FIFO antes de configurar
  mpu.resetFIFO();           // Limpa FIFO                                     
  
  mpu.setAccelFIFOEnabled(true);
  mpu.setXGyroFIFOEnabled(true);
  mpu.setYGyroFIFOEnabled(true);
  mpu.setZGyroFIFOEnabled(true);

  mpu.setFIFOEnabled(true);  // Ativa FIFO
  
  vTaskDelay(pdMS_TO_TICKS(100)); 
  Serial.println("[MPU] Inicialização concluída.");
  return 1;
}

void ler_fifo_e_salvar(void) {
    /*
        * Lê os dados do FIFO do MPU-6500 e salva no buffer.
        * Se o FIFO estiver vazio, não faz nada.
        * Se houver dados, lê até 64 leituras (12 bytes cada) e salva no buffer.
        * Cada leitura contém: timestamp, ax, ay, az, gx, gy, gz.
    */

    uint16_t fifo_count = mpu.getFIFOCount();
    if (fifo_count < 12) return;

    fifo_count = (fifo_count / 12) * 12;
    uint8_t fifo_buffer[fifo_count];
    mpu.getFIFOBytes(fifo_buffer, fifo_count);

    // Timestamp atual
    unsigned long timestamp = micros();

    // evita acesso simultâneo ao buffer_dados
    xSemaphoreTake(mutex_buffer, portMAX_DELAY);

    // Loop para processar todos os blocos de 12 bytes (amostras) lidos do FIFO
    for (int i = 0; i < fifo_count; i += 12) {
      int16_t ax = (fifo_buffer[i] << 8) | fifo_buffer[i + 1];
      int16_t ay = (fifo_buffer[i + 2] << 8) | fifo_buffer[i + 3];
      int16_t az = (fifo_buffer[i + 4] << 8) | fifo_buffer[i + 5];
      int16_t gx = (fifo_buffer[i + 6] << 8) | fifo_buffer[i + 7];
      int16_t gy = (fifo_buffer[i + 8] << 8) | fifo_buffer[i + 9];
      int16_t gz = (fifo_buffer[i + 10] << 8) | fifo_buffer[i + 11];

    //Serial.printf("%lu,%d,%d,%d,%d,%d,%d\n", timestamp, ax, ay, az, gx, gy, gz);

    buffer_index += snprintf((char*)&buffer_dados[buffer_index],
                         BUFFER_SIZE_BYTES - buffer_index,
                         "%lu,%d,%d,%d,%d,%d,%d\n",
                         timestamp, ax, ay, az, gx, gy, gz);
  }
    xSemaphoreGive(mutex_buffer);
}
