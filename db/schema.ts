// =============================================================================
//  db/schema.ts - Tablas del Sistema Cine en Netlify Database (Postgres).
//  Es la misma estructura que database/schema.sql (SQLite), mas dos tablas
//  que en C++ vivian en la memoria del servidor: las sesiones del admin y las
//  reservaciones temporales (en Netlify cada peticion puede correr en una
//  maquina distinta, asi que la memoria no se comparte).
// =============================================================================
import { sql } from "drizzle-orm";
import {
  check, index, integer, jsonb, numeric, pgTable, serial, text, timestamp, unique,
} from "drizzle-orm/pg-core";

export const salas = pgTable("salas", {
  id: serial().primaryKey(),
  nombre: text().notNull().unique(),
  agrupaciones: integer().notNull(),
  butacasPorAgrupacion: integer("butacas_por_agrupacion").notNull(),
  habilitada: integer().notNull().default(1),
}, (t) => [
  check("salas_agrupaciones_check", sql`${t.agrupaciones} > 0`),
  check("salas_butacas_check", sql`${t.butacasPorAgrupacion} > 0`),
  check("salas_habilitada_check", sql`${t.habilitada} IN (0, 1)`),
]);

export const butacas = pgTable("butacas", {
  id: serial().primaryKey(),
  salaId: integer("sala_id").notNull().references(() => salas.id),
  agrupacion: integer().notNull(),
  numero: integer().notNull(),
  habilitada: integer().notNull().default(1),
}, (t) => [
  unique("butacas_posicion_unique").on(t.salaId, t.agrupacion, t.numero),
  index("idx_butacas_sala").on(t.salaId),
  check("butacas_habilitada_check", sql`${t.habilitada} IN (0, 1)`),
]);

export const peliculas = pgTable("peliculas", {
  id: serial().primaryKey(),
  nombre: text().notNull(),
  fechaInicio: text("fecha_inicio").notNull(),   // 'AAAA-MM-DD'
  fechaFin: text("fecha_fin").notNull(),
  precio: numeric({ mode: "number" }).notNull(),
  salaId: integer("sala_id").notNull().references(() => salas.id),
  imagen: text().notNull().default(""),
}, (t) => [
  check("peliculas_precio_check", sql`${t.precio} > 0`),
  check("peliculas_fechas_check", sql`${t.fechaFin} >= ${t.fechaInicio}`),
  index("idx_peliculas_fechas").on(t.fechaInicio, t.fechaFin),
]);

export const funciones = pgTable("funciones", {
  id: serial().primaryKey(),
  peliculaId: integer("pelicula_id").notNull().references(() => peliculas.id),
  hora: text().notNull(),                         // 'HH:MM'
}, (t) => [unique("funciones_pelicula_hora_unique").on(t.peliculaId, t.hora)]);

export const clientes = pgTable("clientes", {
  id: serial().primaryKey(),
  nombre: text().notNull(),
  telefono: text().notNull(),
});

export const reservaciones = pgTable("reservaciones", {
  id: serial().primaryKey(),
  funcionId: integer("funcion_id").notNull().references(() => funciones.id),
  butacaId: integer("butaca_id").notNull().references(() => butacas.id),
  clienteId: integer("cliente_id").notNull().references(() => clientes.id),
  fecha: text().notNull(),                        // dia de la funcion
  fechaHora: timestamp("fecha_hora", { withTimezone: true }).notNull().defaultNow(),
}, (t) => [
  // Una butaca solo se reserva UNA vez por funcion y por dia.
  unique("reservaciones_unica").on(t.funcionId, t.butacaId, t.fecha),
  index("idx_reservaciones_funcion").on(t.funcionId, t.fecha),
]);

export const administradores = pgTable("administradores", {
  id: serial().primaryKey(),
  usuario: text().notNull().unique(),
  sal: text().notNull(),
  claveHash: text("clave_hash").notNull(),
});

// Sesiones del panel admin (antes en memoria: SesionesAdmin).
export const sesionesAdmin = pgTable("sesiones_admin", {
  token: text().primaryKey(),
  usuario: text().notNull(),
  vence: timestamp({ withTimezone: true }).notNull(),
});

// Reservaciones en proceso (antes en memoria: ReservasEnMemoria).
export const reservasTemporales = pgTable("reservas_temporales", {
  token: text().primaryKey(),
  clienteId: integer("cliente_id").notNull(),
  funcionId: integer("funcion_id").notNull(),
  fecha: text().notNull(),
  cantidad: integer().notNull(),
  seleccionadas: integer().notNull().default(0),
  matriz: jsonb().$type<number[][]>().notNull(),
  ids: jsonb().$type<number[][]>().notNull(),
  creada: timestamp({ withTimezone: true }).notNull().defaultNow(),
});
