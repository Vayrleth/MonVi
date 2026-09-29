// gerente.js -- Panel de la Gerencia (gerente.html)

const usuario = localStorage.getItem('skylinea_usuario');
const rol = localStorage.getItem('skylinea_rol');

if (!usuario || rol !== 'GERENTE') {
    window.location.href = 'login.html';
}

document.getElementById('nombreUsuario').textContent = usuario;

document.getElementById('btnLogout').addEventListener('click', (e) => {
    e.preventDefault();
    localStorage.clear();
    window.location.href = 'login.html';
});

const mensajeGlobal = document.getElementById('mensajeGlobal');
function mostrarMensaje(texto, tipo) {
    mensajeGlobal.style.display = 'block';
    mensajeGlobal.className = 'mensaje-estado ' + tipo;
    mensajeGlobal.textContent = texto;
    window.scrollTo({ top: 0, behavior: 'smooth' });
}

// --------------------------- PESTAÑAS -------------------------------------
document.querySelectorAll('.pestañas button').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.pestañas button').forEach(b => b.classList.remove('activa'));
        btn.classList.add('activa');
        document.querySelectorAll('.tab-panel').forEach(p => p.style.display = 'none');
        document.getElementById('tab-' + btn.dataset.tab).style.display = 'block';
    });
});

// --------------------------- VUELOS ---------------------------------------
async function cargarVuelosGerente() {
    const contenedor = document.getElementById('listaVuelosGerente');
    try {
        const resp = await fetch('/api/vuelos');
        const data = await resp.json();
        contenedor.innerHTML = '';
        data.vuelos.forEach(v => {
            const tarjeta = document.createElement('div');
            tarjeta.className = 'tarjeta';
            tarjeta.innerHTML = `
                <h3>#${v.id} ${v.origen} → ${v.destino}</h3>
                <p>Asientos: <strong>${v.asientos}</strong> | Estado: <strong>${v.estado}</strong></p>
                <p class="precio">$${parseFloat(v.precio).toFixed(2)}</p>
                <button class="btn secundario" data-editar='${JSON.stringify(v)}'>Cargar en formulario</button>
            `;
            contenedor.appendChild(tarjeta);
        });
        contenedor.querySelectorAll('button[data-editar]').forEach(btn => {
            btn.addEventListener('click', () => {
                const v = JSON.parse(btn.dataset.editar);
                document.getElementById('vueloId').value = v.id;
                document.getElementById('vueloOrigen').value = v.origen;
                document.getElementById('vueloDestino').value = v.destino;
                document.getElementById('vueloAsientos').value = v.asientos;
                document.getElementById('vueloPrecio').value = v.precio;
                document.getElementById('vueloEstado').value = v.estado;
                window.scrollTo({ top: 0, behavior: 'smooth' });
            });
        });
    } catch (err) {
        mostrarMensaje('No se pudo cargar el catalogo de vuelos.', 'error');
    }
}

document.getElementById('formVuelo').addEventListener('submit', async (e) => {
    e.preventDefault();
    const payload = {
        id: document.getElementById('vueloId').value.trim(),
        origen: document.getElementById('vueloOrigen').value.trim(),
        destino: document.getElementById('vueloDestino').value.trim(),
        asientos: document.getElementById('vueloAsientos').value,
        precio: document.getElementById('vueloPrecio').value,
        estado: document.getElementById('vueloEstado').value
    };

    try {
        const resp = await fetch('/api/gerente/vuelos', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await resp.json();
        if (data.status === 'success') {
            mostrarMensaje('Vuelo guardado correctamente.', 'exito');
            document.getElementById('formVuelo').reset();
            cargarVuelosGerente();
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo guardar el vuelo', 'error');
        }
    } catch (err) {
        mostrarMensaje('Error de conexion al guardar el vuelo.', 'error');
    }
});

// --------------------------- VENDEDORES ------------------------------------
document.getElementById('formAltaVendedor').addEventListener('submit', async (e) => {
    e.preventDefault();
    const usuarioNuevo = document.getElementById('vendUsuario').value.trim();
    const password = document.getElementById('vendPassword').value;

    try {
        const resp = await fetch('/api/gerente/vendedores/agregar', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ usuario: usuarioNuevo, password })
        });
        const data = await resp.json();
        if (data.status === 'success') {
            mostrarMensaje('Vendedor agregado correctamente.', 'exito');
            document.getElementById('formAltaVendedor').reset();
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo agregar el vendedor', 'error');
        }
    } catch (err) {
        mostrarMensaje('Error de conexion al agregar vendedor.', 'error');
    }
});

document.getElementById('formBajaVendedor').addEventListener('submit', async (e) => {
    e.preventDefault();
    const usuarioBaja = document.getElementById('vendUsuarioBaja').value.trim();

    try {
        const resp = await fetch('/api/gerente/vendedores/eliminar', {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ usuario: usuarioBaja })
        });
        const data = await resp.json();
        if (data.status === 'success') {
            mostrarMensaje('Vendedor eliminado. Su historial de ventas se preservo intacto.', 'exito');
            document.getElementById('formBajaVendedor').reset();
            cargarHistorialGlobal();
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo eliminar el vendedor', 'error');
        }
    } catch (err) {
        mostrarMensaje('Error de conexion al eliminar vendedor.', 'error');
    }
});

// --------------------------- HISTORIAL GLOBAL -------------------------------
async function cargarHistorialGlobal() {
    try {
        const resp = await fetch('/api/gerente/historial-general');
        const data = await resp.json();

        const tablaReservas = document.getElementById('tablaHistorialReservasGlobal');
        tablaReservas.innerHTML = '';
        data.historial_reservas.forEach(h => {
            const fila = document.createElement('tr');
            fila.innerHTML = `<td>${h.timestamp}</td><td>${h.cliente}</td><td>${h.idVuelo}</td><td>${h.vendedor}</td><td>${h.estado}</td>`;
            tablaReservas.appendChild(fila);
        });

        const tablaVentas = document.getElementById('tablaHistorialVentasGlobal');
        tablaVentas.innerHTML = '';
        data.historial_ventas.forEach(h => {
            const fila = document.createElement('tr');
            fila.innerHTML = `<td>${h.timestamp}</td><td>${h.vendedor}</td><td>${h.cliente}</td><td>${h.idVuelo}</td><td>${h.comision}</td><td>${h.tipo}</td>`;
            tablaVentas.appendChild(fila);
        });
    } catch (err) {
        mostrarMensaje('No se pudo cargar el historial global.', 'error');
    }
}

// --------------------------- REPORTES (Pipes + fork/exec) ------------------
document.getElementById('btnGenerarReporte').addEventListener('click', async () => {
    const salida = document.getElementById('salidaReporte');
    salida.textContent = 'Generando reporte (proceso independiente via fork+exec+pipes)...';
    try {
        const resp = await fetch('/api/gerente/reportes');
        const data = await resp.json();
        salida.textContent = JSON.stringify(data, null, 2);
    } catch (err) {
        salida.textContent = 'Error al generar el reporte: ' + err.message;
    }
});

cargarVuelosGerente();
cargarHistorialGlobal();
