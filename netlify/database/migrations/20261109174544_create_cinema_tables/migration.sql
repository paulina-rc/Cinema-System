CREATE TABLE "administradores" (
	"id" serial PRIMARY KEY,
	"usuario" text NOT NULL UNIQUE,
	"sal" text NOT NULL,
	"clave_hash" text NOT NULL
);
--> statement-breakpoint
CREATE TABLE "butacas" (
	"id" serial PRIMARY KEY,
	"sala_id" integer NOT NULL,
	"agrupacion" integer NOT NULL,
	"numero" integer NOT NULL,
	"habilitada" integer DEFAULT 1 NOT NULL,
	CONSTRAINT "butacas_posicion_unique" UNIQUE("sala_id","agrupacion","numero"),
	CONSTRAINT "butacas_habilitada_check" CHECK ("habilitada" IN (0, 1))
);
--> statement-breakpoint
CREATE TABLE "clientes" (
	"id" serial PRIMARY KEY,
	"nombre" text NOT NULL,
	"telefono" text NOT NULL
);
--> statement-breakpoint
CREATE TABLE "funciones" (
	"id" serial PRIMARY KEY,
	"pelicula_id" integer NOT NULL,
	"hora" text NOT NULL,
	CONSTRAINT "funciones_pelicula_hora_unique" UNIQUE("pelicula_id","hora")
);
--> statement-breakpoint
CREATE TABLE "peliculas" (
	"id" serial PRIMARY KEY,
	"nombre" text NOT NULL,
	"fecha_inicio" text NOT NULL,
	"fecha_fin" text NOT NULL,
	"precio" numeric NOT NULL,
	"sala_id" integer NOT NULL,
	"imagen" text DEFAULT '' NOT NULL,
	CONSTRAINT "peliculas_precio_check" CHECK ("precio" > 0),
	CONSTRAINT "peliculas_fechas_check" CHECK ("fecha_fin" >= "fecha_inicio")
);
--> statement-breakpoint
CREATE TABLE "reservaciones" (
	"id" serial PRIMARY KEY,
	"funcion_id" integer NOT NULL,
	"butaca_id" integer NOT NULL,
	"cliente_id" integer NOT NULL,
	"fecha" text NOT NULL,
	"fecha_hora" timestamp with time zone DEFAULT now() NOT NULL,
	CONSTRAINT "reservaciones_unica" UNIQUE("funcion_id","butaca_id","fecha")
);
--> statement-breakpoint
CREATE TABLE "reservas_temporales" (
	"token" text PRIMARY KEY,
	"cliente_id" integer NOT NULL,
	"funcion_id" integer NOT NULL,
	"fecha" text NOT NULL,
	"cantidad" integer NOT NULL,
	"seleccionadas" integer DEFAULT 0 NOT NULL,
	"matriz" jsonb NOT NULL,
	"ids" jsonb NOT NULL,
	"creada" timestamp with time zone DEFAULT now() NOT NULL
);
--> statement-breakpoint
CREATE TABLE "salas" (
	"id" serial PRIMARY KEY,
	"nombre" text NOT NULL UNIQUE,
	"agrupaciones" integer NOT NULL,
	"butacas_por_agrupacion" integer NOT NULL,
	"habilitada" integer DEFAULT 1 NOT NULL,
	CONSTRAINT "salas_agrupaciones_check" CHECK ("agrupaciones" > 0),
	CONSTRAINT "salas_butacas_check" CHECK ("butacas_por_agrupacion" > 0),
	CONSTRAINT "salas_habilitada_check" CHECK ("habilitada" IN (0, 1))
);
--> statement-breakpoint
CREATE TABLE "sesiones_admin" (
	"token" text PRIMARY KEY,
	"usuario" text NOT NULL,
	"vence" timestamp with time zone NOT NULL
);
--> statement-breakpoint
CREATE INDEX "idx_butacas_sala" ON "butacas" ("sala_id");--> statement-breakpoint
CREATE INDEX "idx_peliculas_fechas" ON "peliculas" ("fecha_inicio","fecha_fin");--> statement-breakpoint
CREATE INDEX "idx_reservaciones_funcion" ON "reservaciones" ("funcion_id","fecha");--> statement-breakpoint
ALTER TABLE "butacas" ADD CONSTRAINT "butacas_sala_id_salas_id_fkey" FOREIGN KEY ("sala_id") REFERENCES "salas"("id");--> statement-breakpoint
ALTER TABLE "funciones" ADD CONSTRAINT "funciones_pelicula_id_peliculas_id_fkey" FOREIGN KEY ("pelicula_id") REFERENCES "peliculas"("id");--> statement-breakpoint
ALTER TABLE "peliculas" ADD CONSTRAINT "peliculas_sala_id_salas_id_fkey" FOREIGN KEY ("sala_id") REFERENCES "salas"("id");--> statement-breakpoint
ALTER TABLE "reservaciones" ADD CONSTRAINT "reservaciones_funcion_id_funciones_id_fkey" FOREIGN KEY ("funcion_id") REFERENCES "funciones"("id");--> statement-breakpoint
ALTER TABLE "reservaciones" ADD CONSTRAINT "reservaciones_butaca_id_butacas_id_fkey" FOREIGN KEY ("butaca_id") REFERENCES "butacas"("id");--> statement-breakpoint
ALTER TABLE "reservaciones" ADD CONSTRAINT "reservaciones_cliente_id_clientes_id_fkey" FOREIGN KEY ("cliente_id") REFERENCES "clientes"("id");