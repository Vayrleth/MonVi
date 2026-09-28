// cliente.js -- Panel del Cliente (cliente.html)

const usuario = localStorage.getItem('skylinea_usuario');
const rol = localStorage.getItem('skylinea_rol');

if (!usuario || rol !== 'CLIENTE') {
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

// --------------------------------------------------------------------------
// Reservas activas creadas EN ESTA SESION del navegador (guardadas en
// localStorage). El historial permanente del cliente se consulta por
// separado desde el servidor via GET /api/cliente/historial.
// --------------------------------------------------------------------------
function obtenerReservasLocales() {
    return JSON.parse(localStorage.getItem('skylinea_reservas_' + usuario) || '[]');
}
function guardarReservaLocal(idReserva, idVuelo) {
    const reservas = obtenerReservasLocales();
    reservas.push({ idReserva, idVuelo });
    localStorage.setItem('skylinea_reservas_' + usuario, JSON.stringify(reservas));
}
function quitarReservaLocal(idReserva) {
    const reservas = obtenerReservasLocales().filter(r => r.idReserva !== idReserva);
    localStorage.setItem('skylinea_reservas_' + usuario, JSON.stringify(reservas));
}

async function cargarCatalogo() {
    const contenedor = document.getElementById('listaVuelos');
    try {
        const resp = await fetch('/api/vuelos');
        const data = await resp.json();
        contenedor.innerHTML = '';

        data.vuelos.forEach(v => {
            const reservasLocales = obtenerReservasLocales().filter(r => r.idVuelo === v.id);
            const tarjeta = document.createElement('div');
            tarjeta.className = 'tarjeta';
            tarjeta.innerHTML = `
                <h3>${v.origen} → ${v.destino}</h3>
                <p>Asientos disponibles: <strong>${v.asientos}</strong></p>
                <p class="precio">$${parseFloat(v.precio).toFixed(2)}</p>
                <button class="btn" data-id="${v.id}" ${v.asientos <= 0 ? 'disabled' : ''}>
                    ${v.asientos <= 0 ? 'Agotado' : 'Comprar'}
                </button>
                ${reservasLocales.map(r => `
                    <div style="margin-top:8px;font-size:0.8rem;">
                        Reserva ${r.idReserva}
                        <button class="btn peligro" style="padding:6px 12px;font-size:0.75rem;margin-left:6px;" data-cancelar="${r.idReserva}" data-vuelo="${r.idVuelo}">Cancelar</button>
                    </div>
                `).join('')}
            `;
            contenedor.appendChild(tarjeta);
        });

        contenedor.querySelectorAll('button[data-id]').forEach(btn => {
            btn.addEventListener('click', () => reservarVuelo(btn.dataset.id));
        });
        contenedor.querySelectorAll('button[data-cancelar]').forEach(btn => {
            btn.addEventListener('click', () => cancelarReserva(btn.dataset.cancelar, btn.dataset.vuelo));
        });

    } catch (err) {
        mostrarMensaje('No se pudo cargar el catalogo de vuelos.', 'error');
    }
}

async function reservarVuelo(idVuelo) {
    try {
        const resp = await fetch('/api/reservar', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ idVuelo, cliente: usuario, vendedor: '' })
        });
        const data = await resp.json();

        if (data.status === 'success') {
            guardarReservaLocal(data.idReserva, idVuelo);
            mostrarMensaje('¡Reserva confirmada! ID: ' + data.idReserva, 'exito');
            cargarCatalogo();
            cargarHistorial();
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo reservar', 'error');
        }
    } catch (err) {
        mostrarMensaje('Error de conexion al reservar.', 'error');
    }
}

async function cancelarReserva(idReserva, idVuelo) {
    try {
        const resp = await fetch('/api/cancelar', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ idReserva, idVuelo, cliente: usuario })
        });
        const data = await resp.json();

        if (data.status === 'success') {
            quitarReservaLocal(idReserva);
            mostrarMensaje('Reserva cancelada correctamente.', 'exito');
            cargarCatalogo();
            cargarHistorial();
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo cancelar', 'error');
        }
    } catch (err) {
        mostrarMensaje('Error de conexion al cancelar.', 'error');
    }
}

async function cargarHistorial() {
    const tabla = document.getElementById('tablaHistorialCliente');
    try {
        const resp = await fetch('/api/cliente/historial', {
            headers: { 'X-Usuario': usuario }
        });
        const data = await resp.json();
        tabla.innerHTML = '';
        data.historial.forEach(h => {
            const fila = document.createElement('tr');
            fila.innerHTML = `<td>${h.timestamp}</td><td>${h.idVuelo}</td><td>${h.vendedor}</td><td>${h.estado}</td>`;
            tabla.appendChild(fila);
        });
    } catch (err) {
        // Silencioso: el historial es informativo
    }
}

document.getElementById('formComentario').addEventListener('submit', async (e) => {
    e.preventDefault();
    const texto = document.getElementById('textoComentario').value.trim();
    try {
        const resp = await fetch('/api/cliente/comentario', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ usuario, texto })
        });
        const data = await resp.json();
        if (data.status === 'success') {
            mostrarMensaje('Gracias por tu comentario.', 'exito');
            document.getElementById('textoComentario').value = '';
        }
    } catch (err) {
        mostrarMensaje('No se pudo enviar el comentario.', 'error');
    }
});

cargarCatalogo();
cargarHistorial();
