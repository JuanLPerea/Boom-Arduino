/*
      Diseño y programación por: Juan Luis Perea López (2019)

      Simulación de juego de Boom con Arduino
      ---------------------------------------
      Pantalla LCD 2004   -------------- Pines A4 y A5 (I2C)
      Lector de tarjetas SD    --------- MOSI PIN D11 -- SCK PIN D13 -- MISO PIN D12 -- CS PIN D4
      Cables de la bomba --------------- PIN D9, D2, D3, D5, D6
      Relay para encender la mecha ----- PIN D7
      Botón para la pantalla LCD ------- PIN D1
      Buzzer --------------------------- PIN D8
      Leds para indicar luz cables ----- D10, A0, A1, A2, A3


*/
#include <Wire.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>


// Definimos la pantalla LCD de 20 x 4 caracteres
LiquidCrystal_I2C lcd(0x3F, 20, 4);


// Archivos en la SD
File posicionFile;
File preguntasFile;

// Variables
int ultimaPosicion = 0;
int fase = 0;
String respuesta[6] =  {"1", "2", "3", "4", "5", "0"};
int respuestaCorrecta = 0;
int pasoPantalla = 0;
long crono;
long cuentaatras;
long ticks;
bool flag = 0;
long nivel = 60000;   // milisegundos que tenemos para desactivar la bomba
int aciertos = 0;

// constantes para los pines de los leds y del botón y el altavoz
const int botonPin = 1;

// pines para las luces led
#define  ledVerdePin  10
#define  ledNaranjaPin  14
#define  ledMoradoPin  15
#define  ledAzulPin  16
#define  ledAmarilloPin  17

// pines para los cables
#define  cableVerde  9
#define  cableNaranja  6
#define  cableMorado  2
#define  cableAzul  5
#define  cableAmarillo  3

// pines para el Altavoz y el relé
#define BuzzerPin  8
#define relayPin  7

// Estado del botón y de los cables
bool estadoBoton = 0;

bool estadoCableVerde = 0;
bool estadoCableNaranja = 0;
bool estadoCableMorado = 0;
bool estadoCableAzul = 0;
bool estadoCableAmarillo = 0;

bool usadoVerde = false;
bool usadoNaranja = false;
bool usadoMorado = false;
bool usadoAzul = false;
bool usadoAmarillo = false;

// Setup -----------------------------------------------------------

void setup()
{
  pinMode(botonPin, INPUT);

  pinMode(cableVerde, INPUT_PULLUP);
  pinMode(cableNaranja, INPUT_PULLUP);
  pinMode(cableMorado, INPUT_PULLUP);
  pinMode(cableAzul, INPUT_PULLUP);
  pinMode(cableAmarillo, INPUT_PULLUP);

  pinMode(ledVerdePin, OUTPUT);
  pinMode(ledNaranjaPin, OUTPUT);
  pinMode(ledMoradoPin, OUTPUT);
  pinMode(ledAzulPin, OUTPUT);
  pinMode(ledAmarilloPin, OUTPUT);


  // Relé
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  //   Serial.begin(9600);
  // Inicializar el LCD
  lcd.init();
  lcd.backlight();

  //  lcd.setCursor(0,0);
  //  lcd.print("Iniciando SD ...");

  if (!SD.begin(4)) {
    lcd.setCursor(0, 0);
    lcd.print("No se pudo");
    lcd.setCursor(0, 1);
    lcd.print("inicializar");
    while (1);

    //return;
  } else {
    //    lcd.setCursor(0,2);
    //    lcd.print("Inicializacion OK!");
  }

  //  delay(1000);

  // Probamos el altavoz
  tone(8, 400, 250);

  //  delay(1000);
  lcd.clear();
  pantallaInicio();

}

void loop() {

  switch (fase) {

    // Primero mostramos en pantalla un mensaje para iniciar el juego y esperamos a que se pulse el botón
    case 0:

      estadoBoton = digitalRead(botonPin);
      leerCables();

      if (ticks < millis() && ticks != 0) {
        pantallaInicio();
        ticks = 0;
      }

      // si pulsamos el botón pasamos a jugar
      if (estadoBoton == HIGH && ticks < millis()) {

        if (estadoCableVerde == HIGH && estadoCableNaranja == HIGH && estadoCableMorado == HIGH && estadoCableAzul == HIGH && estadoCableAmarillo == HIGH) {
          fase = 1;
          lcd.clear();
        } else {
          lcd.clear();
          pantallaCables();
          ticks = millis() + 3000;
        }

      }
      break;

    // Cargamos la pregunta
    case 1:
      leerPosicion();
      cargarPreguntas();
      grabarPosicion();

      // Si el texto a mostrar es mas largo de lo que cabe en 1 pantalla, dar mas tiempo para  mostrarlo en 2 pantallas
      if (respuesta[pasoPantalla].length() > 80) {
        ticks = millis() + 6000;
      } else {
        ticks = millis() + 3000;
      }

      imprimirLcd(respuesta[0]);
      for (int r = 0; r < 4 ; r++) {
        for (int n = 100 ; n < 1000 ; n += 1) {
          tone(8, n, 100);
          delay(1);
        }
      }

      while (millis() < ticks) {
        imprimirLcd(respuesta[0]);
      }

      ticks = 0;

      imprimirLcd("Tiempo!!");
      delay(1000);

      fase = 2;
      cuentaatras = millis() + nivel;

      break;

    // Fase principal, iniciamos el cronómetro y comprobamos los cables,
    // si se desconecta el de la respuesta correcta, hacemos que explote la bomba
    // Si se desconectan todos menos el correcto, gana el jugador
    case 2:
      // mientras que estemos dentro del tiempo, comprobar si el jugador desconecta algún cable
      while (millis() < cuentaatras) {

        // alarma cuando falten menos de 10 segundos
        crono = (int)((cuentaatras - millis()) / 1000);
        if (crono < 10 && crono % 2 == 0) {
          tone(8, 1000, 200);
        }


        // si no pulsamos el botón en 3 segundos, mostrar la cuenta atrás
        // si hemos pulsado el botón, mostrar la pregunta o las respuestas
        if (ticks < millis()) {
          lcd.clear();
          lcd.setCursor(4, 1);
          lcd.print("Explosion en:");
          lcd.setCursor(10, 2);
          lcd.print(crono);
          ticks = 0;
        } else {
          if (flag) {
            imprimirLcd(respuesta[pasoPantalla]);
            flag = false;
          }

        }


        // si el jugador pulsa el botón, cambiar en la pantalla entre la pregunta y las respuestas
        estadoBoton = digitalRead(botonPin);
        delay(100);
        if (estadoBoton == HIGH) {
          pasoPantalla++;
          if (pasoPantalla == 6) pasoPantalla = 0;
          encenderLeds(pasoPantalla);

          // Si el texto a mostrar es mas largo de lo que cabe en 1 pantalla, dar mas tiempo para  mostrarlo en 2 pantallas
          if (respuesta[pasoPantalla].length() > 80) {
            ticks = millis() + 6000;
          } else {
            ticks = millis() + 3000;
          }

          flag = true;
        }

        // detectar si desconectamos algún cable

        estadoCableVerde = digitalRead(cableVerde);
        estadoCableNaranja = digitalRead(cableNaranja);
        estadoCableMorado = digitalRead(cableMorado);
        estadoCableAzul = digitalRead(cableAzul);
        estadoCableAmarillo = digitalRead(cableAmarillo);


        if (estadoCableVerde == LOW && usadoVerde == false) {
          cableDesconectado(1, crono);
          usadoVerde = true;
        }
        if (estadoCableNaranja == LOW && usadoNaranja == false) {
          cableDesconectado(2, crono);
          usadoNaranja = true;
        }
        if (estadoCableMorado == LOW && usadoMorado == false) {
          cableDesconectado(3, crono);
          usadoMorado = true;
        }
        if (estadoCableAzul == LOW && usadoAzul == false) {
          cableDesconectado(4, crono);
          usadoAzul = true;
        }
        if (estadoCableAmarillo == LOW && usadoAmarillo == false) {
          cableDesconectado(5, crono);
          usadoAmarillo = true;
        }

        delay(100);

      }

      // Cuando sale de este bucle es que se ha acabado el tiempo
      // y explota la bomba o que hemos ganado (y la fase vuelve a 0)

      if (fase == 2) {
        pantallaBoom();
      }

      break;

  }

  // *************************************************************************************************************************
  // *************************************************************************************************************************
  // *************************************************************************************************************************
  // *************************************************************************************************************************

}

void cargarPreguntas() {
  preguntasFile = SD.open("preg.txt");       //abrimos  el archivo
  long totalBytes = preguntasFile.size();
  String cadena = "";

  if (preguntasFile) {

    char caracter = 0x00;          // vaciamos la variable caracter

    if (ultimaPosicion >= totalBytes - 2)   {
      ultimaPosicion = 0;
    }

    //
    //      lcd.print("total Bytes ");
    //      lcd.print(totalBytes);
    //
    //      lcd.print("Seek ");
    //      lcd.print(ultimaPosicion);


    for (int n = 0 ; n < ultimaPosicion ; n++) {
      caracter = preguntasFile.read();
    }


    // Comprobamos que la última posición guardada en la tarjeta SD corresponda con
    // el inicio de un bloque de pregunta. Esto lo identificamos porque empieza con el
    // carácter '*'


    // Buscamos el inicio de una pregunta...
    while (caracter != '*') {
      caracter = preguntasFile.read();
      if (caracter == -1) {
        break;
      }
    }

    // Leemos 7 líneas diferentes, La primera es la pregunta, y después las 5 respuestas y la última es el número de la respuesta correcta
    for (int n = 0 ; n < 7 ; n++) {
      do {
        caracter = preguntasFile.read();
        if (caracter != 10) {
          cadena = cadena + caracter;
        }


      } while (caracter != 10);

      respuesta[n] = cadena;
      cadena = "";
    }

    // Aquí guardamos el número de la respuesta correcta para luego saber si acertamos
    respuestaCorrecta = respuesta[6].toInt();
    ultimaPosicion = preguntasFile.position();

    //      lcd.clear();
    //      lcd.print("Cargada Pregunta ");
    //      lcd.print(ultimaPosicion);
    //      delay(1000);

    //---------------------------------------------------
    preguntasFile.close(); //cerramos el archivo

  } else {
    //      lcd.clear();
    //      lcd.print("Error archivo Preguntas");
    //      delay(3000);
  }
  delay(1000);
}


void grabarPosicion() {
  SD.remove("posicion.txt");                      // borramos el archivo de la tarjeta SD
  posicionFile = SD.open("posicion.txt", FILE_WRITE);   // creamos un nuevo archivo para guardar la posición

  if (posicionFile) {
    //  //  Serial.print("Escribiendo SD: ");
    posicionFile.print(ultimaPosicion);
    posicionFile.print("#");                         // insertamos un caracter para detectar el final de los datos
    posicionFile.close(); //cerramos el archivo

    //      lcd.clear();
    //      lcd.print("Grabada Posición ");
    //      lcd.print(ultimaPosicion);
    //
    //      delay(1000);

  } else {
    //      lcd.clear();
    //      lcd.print("Error al Grabar Posición");
    //      delay(1000);
  }
  delay(100);
}

void leerPosicion() {
  posicionFile = SD.open("posicion.txt");           //abrimos  el archivo
  int totalBytes = posicionFile.size();

  String cadena = "";

  if (posicionFile) {


    //--Leemos una línea de la hoja de texto--------------
    while (posicionFile.available()) {

      char caracter = posicionFile.read();
      if (caracter == '#')   //  caracter al final
      {
        break;
      }
      cadena = cadena + caracter;


    }
    //---------------------------------------------------
    posicionFile.close(); //cerramos el archivo

    ultimaPosicion = cadena.toInt();
    //  lcd.clear();
    //  lcd.print("Leida posicion: ");
    //  lcd.print(ultimaPosicion);
    //  delay(3000);

  } else {
    //  lcd.clear();
    //  lcd.print("Falta archivo posicion");
    //  delay(3000);
  }
}

// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************



// Aquí comprobamos si el cable que hemos desconectado es el correcto o no (y si es así explota la bomba!!!)
void cableDesconectado (int cable, long pausacrono) {

  // sonidos, luces y esperamos un segundo para darle emoción ...
  for (int n = 0 ; n < 5 ; n++) {
    tone(8, 600, 100);
    delay(100);
  }
  tone(8, 300, 200);
  delay(500);


  if (cable != respuestaCorrecta) {
    // Hemos acertado, detectar si hemos desactivado la bomba o continúa habiendo cables por desconectar
    aciertos++;
    if (aciertos == 4) {
      pantallaGanar();
    } else {
      pantallaContinua();
      cuentaatras = (pausacrono * 1000) + millis();
      ticks = millis() + 1000;
    }

    // Error, la bomba explota
  } else {
    pantallaBoom();
  }

}

void pantallaContinua() {
  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print("CORRECTO");
  lcd.setCursor(5, 2);
  lcd.print("CONTINUA");

  tone(8, 600, 200);
  //imprimirLcd(respuesta[pasoPantalla]);
}

void pantallaGanar() {
  lcd.clear();
  lcd.setCursor(7, 1);
  lcd.print("BOMBA");
  lcd.setCursor(4, 2);
  lcd.print("DESACTIVADA");

  for (int r = 0 ; r < 10 ; r++) {
    for (int n = 100 ; n < 500 ; n++) {
      tone(8, n, 100);
    }
  }

  // Esperar a que el jugador pulse el botón
  estadoBoton = LOW;
  while (estadoBoton != HIGH) {
    estadoBoton = digitalRead(botonPin);
    delay(100);
  }

  reiniciar();
}

void pantallaInicio() {
  lcd.clear();
  lcd.print("PULSA EL BOTON      PARA EMPEZAR");
}

void pantallaCables() {
  lcd.clear();
  lcd.print("CONECTA TODOS       LOS CABLES");

  for (int n = 100 ; n < 1000 ; n++) {
    tone(8, n, 100);
  }
}

void pantallaBoom() {
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("BOOOOOOOOOOOOOM");

  // Activamos el relé
  digitalWrite(relayPin, HIGH);

  for (int n = 0 ; n < 1000 ; n++) {
    tone(8, 100, 100);
    delay(2);
  }

  // Desactivamos el relé
  digitalWrite(relayPin, LOW);


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("La respuesta        correcta era...");
  delay(3000);

  // Encender el led que corresponda
  digitalWrite(ledVerdePin, LOW);
  digitalWrite(ledNaranjaPin, LOW);
  digitalWrite(ledMoradoPin, LOW);
  digitalWrite(ledAzulPin, LOW);
  digitalWrite(ledAmarilloPin, LOW);

  switch (respuestaCorrecta) {
    case 1:
      digitalWrite(ledVerdePin, HIGH);
      break;
    case 2:
      digitalWrite(ledNaranjaPin, HIGH);
      break;
    case 3:
      digitalWrite(ledMoradoPin, HIGH);
      break;
    case 4:
      digitalWrite(ledAzulPin, HIGH);
      break;
    case 5:
      digitalWrite(ledAmarilloPin, HIGH);
      break;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  imprimirLcd(respuesta[respuestaCorrecta]);

  // Esperar a que el jugador pulse el botón
  estadoBoton = LOW;
  while (estadoBoton != HIGH) {
    estadoBoton = digitalRead(botonPin);
    delay(100);
  }

  reiniciar();

}

// Imprimir los textos de pregunta y respuestas en la pantalla ----------------------------------------------------------
void imprimirLcd(String cadena) {
  lcd.clear();

  if (cadena.length() >= 80) {
    if (ticks > 3000) {
      cadena = cadena.substring(0, 80);
    } else {
      cadena = cadena.substring(80, cadena.length());
    }
  } else {

    if (cadena.length() >= 60) {
      lcd.setCursor(0, 3);
      lcd.print(cadena.substring(60, 80));
    }
    if (cadena.length() >= 40) {
      lcd.setCursor(0, 2);
      lcd.print(cadena.substring(40, 60));
    }
    if (cadena.length() >= 20) {
      lcd.setCursor(0, 1);
      lcd.print(cadena.substring(20, 40));
    }
    if (cadena.length() > 0) {
      lcd.setCursor(0, 0);
      lcd.print(cadena.substring(0, 20));
    }
  }


}

void leerCables() {

  estadoCableVerde = digitalRead(cableVerde);
  estadoCableNaranja = digitalRead(cableNaranja);
  estadoCableMorado = digitalRead(cableMorado);
  estadoCableAzul = digitalRead(cableAzul);
  estadoCableAmarillo = digitalRead(cableAmarillo);
  delay(100);

  cablesConectados();
}


void encenderLeds(int paso) {

  switch (paso) {
    case 0:
      leerCables();
      break;

    case 1:
      if (usadoVerde == false) {
        digitalWrite(ledVerdePin, HIGH);
      }
      digitalWrite(ledNaranjaPin, LOW);
      digitalWrite(ledMoradoPin, LOW);
      digitalWrite(ledAzulPin, LOW);
      digitalWrite(ledAmarilloPin, LOW);
      break;

    case 2:
      if (usadoNaranja == false) {
        digitalWrite(ledNaranjaPin, HIGH);
      }
      digitalWrite(ledVerdePin, LOW);
      digitalWrite(ledMoradoPin, LOW);
      digitalWrite(ledAzulPin, LOW);
      digitalWrite(ledAmarilloPin, LOW);
      break;

    case 3:
      if (usadoMorado == false) {
        digitalWrite(ledMoradoPin, HIGH);
      }
      digitalWrite(ledVerdePin, LOW);
      digitalWrite(ledNaranjaPin, LOW);
      digitalWrite(ledAzulPin, LOW);
      digitalWrite(ledAmarilloPin, LOW);
      break;

    case 4:
      if (usadoAzul == false) {
        digitalWrite(ledAzulPin, HIGH);
      }
      digitalWrite(ledVerdePin, LOW);
      digitalWrite(ledNaranjaPin, LOW);
      digitalWrite(ledMoradoPin, LOW);
      digitalWrite(ledAmarilloPin, LOW);
      break;

    case 5:
      if (usadoAmarillo == false) {
        digitalWrite(ledAmarilloPin, HIGH);
      }
      digitalWrite(ledVerdePin, LOW);
      digitalWrite(ledNaranjaPin, LOW);
      digitalWrite(ledMoradoPin, LOW);
      digitalWrite(ledAzulPin, LOW);
      break;

  }
}


void cablesConectados() {
  if (estadoCableVerde == HIGH) {
    digitalWrite(ledVerdePin, HIGH);
  } else {
    digitalWrite(ledVerdePin, LOW);
  }
  if (estadoCableNaranja == HIGH) {
    digitalWrite(ledNaranjaPin, HIGH);
  } else {
    digitalWrite(ledNaranjaPin, LOW);
  }
  if (estadoCableMorado == HIGH) {
    digitalWrite(ledMoradoPin, HIGH);
  } else {
    digitalWrite(ledMoradoPin, LOW);
  }
  if (estadoCableAzul == HIGH) {
    digitalWrite(ledAzulPin, HIGH);
  } else {
    digitalWrite(ledAzulPin, LOW);
  }
  if (estadoCableAmarillo == HIGH) {
    digitalWrite(ledAmarilloPin, HIGH);
  } else {
    digitalWrite(ledAmarilloPin, LOW);
  }
}

void reiniciar() {

  // Borrar la SRAM
  asm volatile ("  jmp 0");

  usadoVerde = false;
  usadoNaranja = false;
  usadoMorado = false;
  usadoAzul = false;
  usadoAmarillo = false;

  fase = 0;
  cuentaatras = 0;
  aciertos = 0;
  pasoPantalla = 0;
  delay(1000);
  pantallaInicio();


}

