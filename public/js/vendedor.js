// vendedor.js -- Panel del Vendedor (vendedor.html)

const usuario = localStorage.getItem('skylinea_usuario');
const rol = localStorage.getItem('skylinea_rol');

if (!usuario || rol !== 'VENDEDOR') {
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

async function cargarVuelosParaVenta() {
    const select = document.getElementById('ventaVuelo');
    try {
        const resp = await fetch('/api/vuelos');
        const data = await resp.json();
        select.innerHTML = '';
        data.vuelos.forEach(v => {
            const opcion = document.createElement('option');
            opcion.value = v.id;
            opcion.textContent = `#${v.id} ${v.origen} → ${v.destino} ($${parseFloat(v.precio).toFixed(2)}, ${v.asientos} disp.)`;
            select.appendChild(opcion);
        });
    } catch (err) {
        mostrarMensaje('No se pudo cargar el catalogo de vuelos.', 'error');
    }
}

document.getElementById('formVenta').addEventListener('submit', async (e) => {
    e.preventDefault();
    const cliente = document.getElementById('ventaCliente').value.trim();
    const idVuelo = document.getElementById('ventaVuelo').value;

    try {
        const resp = await fetch('/api/reservar', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ idVuelo, cliente, vendedor: usuario })
        });
        const data = await resp.json();

        if (data.status === 'success') {
            mostrarMensaje('Venta registrada. ID de reserva: ' + data.idReserva, 'exito');
            document.getElementById('formVenta').reset();
            cargarVuelosParaVenta();
            cargarHistorialVendedor();
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo registrar la venta', 'error');
        }
    } catch (err) {
        mostrarMensaje('Error de conexion al registrar la venta.', 'error');
    }
});

document.getElementById('formTransferencia').addEventListener('submit', async (e) => {
    e.preventDefault();
    const idReserva = document.getElementById('idReservaTransferir').value.trim();
    const vueloOrigen = document.getElementById('vueloOrigenTransferir').value.trim();
    const vueloDestino = document.getElementById('vueloDestinoTransferir').value.trim();
    const cliente = document.getElementById('clienteTransferir').value.trim();

    try {
        const resp = await fetch('/api/vendedor/transferir', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ idReserva, vueloOrigen, vueloDestino, cliente, vendedor: usuario })
        });
        const data = await resp.json();

        if (data.status === 'success') {
            mostrarMensaje('Transferencia realizada. Nueva reserva: ' + data.nuevaReserva, 'exito');
            document.getElementById('formTransferencia').reset();
            cargarVuelosParaVenta();
            cargarHistorialVendedor();
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo transferir la reserva', 'error');
        }
    } catch (err) {
        mostrarMensaje('Error de conexion al transferir.', 'error');
    }
});

async function cargarHistorialVendedor() {
    const tabla = document.getElementById('tablaHistorialVendedor');
    try {
        const resp = await fetch('/api/vendedor/historial', {
            headers: { 'X-Usuario': usuario }
        });
        const data = await resp.json();
        tabla.innerHTML = '';
        data.historial.forEach(h => {
            const fila = document.createElement('tr');
            fila.innerHTML = `<td>${h.timestamp}</td><td>${h.cliente}</td><td>${h.idVuelo}</td><td>${h.comision}</td><td>${h.tipo}</td>`;
            tabla.appendChild(fila);
        });
    } catch (err) {
        // Silencioso
    }
}

cargarVuelosParaVenta();
cargarHistorialVendedor();
