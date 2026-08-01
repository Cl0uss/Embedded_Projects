#!/usr/bin/env python3

import math
import tkinter as tk
from tkinter import messagebox
from tkinter import ttk
from typing import Optional

import serial
from serial.tools import list_ports


SERIAL_BAUD_RATE = 115200
UPDATE_INTERVAL_MS = 20

# The dot reaches the circle edge at this angle.
MAX_TILT_DEGREES = 45.0


class MPU6050Viewer:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("MPU-6050 Tilt Viewer")
        self.root.geometry("560x680")
        self.root.minsize(440, 540)

        self.serial_connection: Optional[serial.Serial] = None
        self.receive_buffer = ""

        # Values are received directly from ESP32.
        self.pitch = 0.0
        self.roll = 0.0

        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(
            value="Disconnected"
        )
        self.raw_line_var = tk.StringVar(
            value="No serial data received"
        )

        self.pitch_var = tk.StringVar(
            value="Pitch: 0.0°"
        )
        self.roll_var = tk.StringVar(
            value="Roll: 0.0°"
        )
        self.tilt_strength_var = tk.StringVar(
            value="Tilt strength: 0.0°"
        )

        self.build_interface()
        self.refresh_ports()

        self.root.after(
            UPDATE_INTERVAL_MS,
            self.update_app,
        )

        self.root.protocol(
            "WM_DELETE_WINDOW",
            self.close_app,
        )

    def build_interface(self) -> None:
        main_frame = ttk.Frame(
            self.root,
            padding=16,
        )
        main_frame.pack(
            fill=tk.BOTH,
            expand=True,
        )

        connection_frame = ttk.LabelFrame(
            main_frame,
            text="Serial connection",
            padding=10,
        )
        connection_frame.pack(fill=tk.X)

        self.port_combo = ttk.Combobox(
            connection_frame,
            textvariable=self.port_var,
            state="readonly",
        )
        self.port_combo.pack(
            side=tk.LEFT,
            fill=tk.X,
            expand=True,
        )

        ttk.Button(
            connection_frame,
            text="Refresh",
            command=self.refresh_ports,
        ).pack(
            side=tk.LEFT,
            padx=(8, 0),
        )

        self.connect_button = ttk.Button(
            connection_frame,
            text="Connect",
            command=self.toggle_connection,
        )
        self.connect_button.pack(
            side=tk.LEFT,
            padx=(8, 0),
        )

        ttk.Label(
            main_frame,
            textvariable=self.status_var,
            anchor=tk.CENTER,
        ).pack(
            fill=tk.X,
            pady=(8, 2),
        )

        ttk.Label(
            main_frame,
            textvariable=self.raw_line_var,
            anchor=tk.CENTER,
        ).pack(
            fill=tk.X,
            pady=(2, 8),
        )

        tilt_frame = ttk.LabelFrame(
            main_frame,
            text="Tilt direction and strength",
            padding=10,
        )
        tilt_frame.pack(
            fill=tk.BOTH,
            expand=True,
        )

        self.tilt_canvas = tk.Canvas(
            tilt_frame,
            background="#17191c",
            highlightthickness=0,
        )
        self.tilt_canvas.pack(
            fill=tk.BOTH,
            expand=True,
        )

        self.tilt_canvas.bind(
            "<Configure>",
            lambda _event: self.draw_tilt(),
        )

        values_frame = ttk.Frame(tilt_frame)
        values_frame.pack(
            fill=tk.X,
            pady=(8, 0),
        )

        ttk.Label(
            values_frame,
            textvariable=self.pitch_var,
            font=("Sans", 12),
        ).pack(
            side=tk.LEFT,
            expand=True,
        )

        ttk.Label(
            values_frame,
            textvariable=self.roll_var,
            font=("Sans", 12),
        ).pack(
            side=tk.LEFT,
            expand=True,
        )

        ttk.Label(
            tilt_frame,
            textvariable=self.tilt_strength_var,
            anchor=tk.CENTER,
            font=("Sans", 12),
        ).pack(
            fill=tk.X,
            pady=(5, 0),
        )

    def refresh_ports(self) -> None:
        ports = [
            port.device
            for port in list_ports.comports()
        ]

        self.port_combo["values"] = ports

        if not ports:
            self.port_var.set("")
            return

        if self.port_var.get() in ports:
            return

        preferred_port = next(
            (
                port
                for port in ports
                if "ttyACM" in port
                or "ttyUSB" in port
            ),
            ports[0],
        )

        self.port_var.set(preferred_port)

    def toggle_connection(self) -> None:
        if (
            self.serial_connection is not None
            and self.serial_connection.is_open
        ):
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        port = self.port_var.get()

        if not port:
            messagebox.showerror(
                "No serial port",
                "Select a serial port first.",
            )
            return

        try:
            self.serial_connection = serial.Serial(
                port=port,
                baudrate=SERIAL_BAUD_RATE,
                timeout=0,
            )

            self.serial_connection.reset_input_buffer()

        except serial.SerialException as error:
            messagebox.showerror(
                "Connection error",
                str(error),
            )
            return

        self.receive_buffer = ""

        self.status_var.set(
            f"Connected: {port}"
        )

        self.raw_line_var.set(
            "Waiting for ESP32 data..."
        )

        self.connect_button.configure(
            text="Disconnect"
        )

    def disconnect(self) -> None:
        if self.serial_connection is not None:
            try:
                self.serial_connection.close()
            except serial.SerialException:
                pass

        self.serial_connection = None

        self.status_var.set("Disconnected")

        self.connect_button.configure(
            text="Connect"
        )

    def read_serial_data(self) -> None:
        if self.serial_connection is None:
            return

        if not self.serial_connection.is_open:
            return

        try:
            bytes_available = (
                self.serial_connection.in_waiting
            )

            if bytes_available <= 0:
                return

            data = self.serial_connection.read(
                bytes_available
            ).decode(
                "utf-8",
                errors="ignore",
            )

            data = data.replace("\r", "\n")
            self.receive_buffer += data

            while "\n" in self.receive_buffer:
                line, self.receive_buffer = (
                    self.receive_buffer.split(
                        "\n",
                        1,
                    )
                )

                line = line.strip()

                if line:
                    self.parse_serial_line(line)

        except serial.SerialException as error:
            self.disconnect()

            self.status_var.set(
                f"Serial error: {error}"
            )

    def parse_serial_line(self, line: str) -> None:
        self.raw_line_var.set(line[:120])

        if line.startswith("STATUS,"):
            self.status_var.set(
                line.split(",", 1)[1]
            )
            return

        if line.startswith("CALIBRATION,"):
            self.status_var.set(
                "Calibration: " +
                line.split(",", 1)[1]
            )
            return

        if line.startswith("ERROR,"):
            self.status_var.set(
                "ESP32 error: " +
                line.split(",", 1)[1]
            )
            return

        if not line.startswith("DATA,"):
            return

        parts = line.split(",")

        if len(parts) != 3:
            self.status_var.set(
                f"Invalid DATA field count: {len(parts)}"
            )
            return

        try:
            # No sensor logic is calculated in Python.
            self.pitch = float(parts[1])
            self.roll = float(parts[2])

        except ValueError:
            self.status_var.set(
                "Invalid numeric DATA"
            )

    def update_labels(self) -> None:
        self.pitch_var.set(
            f"Pitch: {self.pitch:.1f}°"
        )

        self.roll_var.set(
            f"Roll: {self.roll:.1f}°"
        )

        tilt_strength = math.hypot(
            self.pitch,
            self.roll,
        )

        self.tilt_strength_var.set(
            f"Tilt strength: {tilt_strength:.1f}°"
        )

    def update_app(self) -> None:
        self.read_serial_data()
        self.update_labels()
        self.draw_tilt()

        self.root.after(
            UPDATE_INTERVAL_MS,
            self.update_app,
        )

    def draw_base_circle(
        self,
    ) -> tuple[float, float, float]:
        width = self.tilt_canvas.winfo_width()
        height = self.tilt_canvas.winfo_height()

        if width < 10 or height < 10:
            return 0.0, 0.0, 0.0

        self.tilt_canvas.delete("all")

        center_x = width / 2
        center_y = height / 2
        radius = min(width, height) * 0.36

        self.tilt_canvas.create_oval(
            center_x - radius,
            center_y - radius,
            center_x + radius,
            center_y + radius,
            outline="#d8dde5",
            width=3,
        )

        for scale in (0.25, 0.50, 0.75):
            ring_radius = radius * scale

            self.tilt_canvas.create_oval(
                center_x - ring_radius,
                center_y - ring_radius,
                center_x + ring_radius,
                center_y + ring_radius,
                outline="#3d444e",
                width=1,
            )

        self.tilt_canvas.create_line(
            center_x - radius,
            center_y,
            center_x + radius,
            center_y,
            fill="#59616c",
            width=1,
        )

        self.tilt_canvas.create_line(
            center_x,
            center_y - radius,
            center_x,
            center_y + radius,
            fill="#59616c",
            width=1,
        )

        self.tilt_canvas.create_oval(
            center_x - 4,
            center_y - 4,
            center_x + 4,
            center_y + 4,
            fill="#d8dde5",
            outline="",
        )

        self.tilt_canvas.create_text(
            center_x,
            center_y - radius - 17,
            text="FORWARD",
            fill="#aeb6c2",
            font=("Sans", 9),
        )

        self.tilt_canvas.create_text(
            center_x,
            center_y + radius + 17,
            text="BACK",
            fill="#aeb6c2",
            font=("Sans", 9),
        )

        self.tilt_canvas.create_text(
            center_x - radius - 27,
            center_y,
            text="LEFT",
            fill="#aeb6c2",
            font=("Sans", 9),
        )

        self.tilt_canvas.create_text(
            center_x + radius + 29,
            center_y,
            text="RIGHT",
            fill="#aeb6c2",
            font=("Sans", 9),
        )

        return center_x, center_y, radius

    def draw_tilt(self) -> None:
        center_x, center_y, radius = (
            self.draw_base_circle()
        )

        if radius <= 0:
            return

        normalized_x = (
            self.roll / MAX_TILT_DEGREES
        )

        normalized_y = (
            self.pitch / MAX_TILT_DEGREES
        )

        vector_length = math.hypot(
            normalized_x,
            normalized_y,
        )

        if vector_length > 1.0:
            normalized_x /= vector_length
            normalized_y /= vector_length

        dot_x = (
            center_x +
            normalized_x * radius
        )

        dot_y = (
            center_y +
            normalized_y * radius
        )

        dot_radius = max(
            10.0,
            radius * 0.065,
        )

        self.tilt_canvas.create_line(
            center_x,
            center_y,
            dot_x,
            dot_y,
            fill="#6ca7ff",
            width=3,
        )

        self.tilt_canvas.create_oval(
            dot_x - dot_radius,
            dot_y - dot_radius,
            dot_x + dot_radius,
            dot_y + dot_radius,
            fill="#6ca7ff",
            outline="#ffffff",
            width=2,
        )

    def close_app(self) -> None:
        self.disconnect()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    MPU6050Viewer(root)
    root.mainloop()


if __name__ == "__main__":
    main()