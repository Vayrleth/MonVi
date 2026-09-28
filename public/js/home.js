// home.js -- Landing Page publica (index.html)
// Consume GET /api/vuelos/publico y renderiza catalogo + testimonios.

async function cargarDatosPublicos() {
    const contenedorVuelos = document.getElementById('listaVuelosPublicos');
    const contenedorTestimonios = document.getElementById('listaTestimonios');
    const mensajeCarga = document.getElementById('mensajeCarga');

    try {
        const resp = await fetch('/api/vuelos/publico');
        const data = await resp.json();

        if (data.status !== 'success') throw new Error(data.mensaje || 'Error desconocido');

        contenedorVuelos.innerHTML = '';
        data.vuelos.forEach(v => {
            const tarjeta = document.createElement('div');
            tarjeta.className = 'tarjeta';
            tarjeta.innerHTML = `
                <h3>${v.origen} → ${v.destino}</h3>
                <p>Asientos disponibles: <strong>${v.asientos}</strong></p>
                <p class="precio">$${parseFloat(v.precio).toFixed(2)}</p>
                <a href="login.html" class="btn" style="margin-top:10px;display:inline-block;">Reservar</a>
            `;
            contenedorVuelos.appendChild(tarjeta);
        });

        contenedorTestimonios.innerHTML = '';
        if (data.testimonios.length === 0) {
            contenedorTestimonios.innerHTML = '<p>Aun no hay comentarios de clientes.</p>';
        } else {
            data.testimonios.forEach(t => {
                const p = document.createElement('p');
                p.className = 'testimonio';
                p.innerHTML = `"${t.texto}" — <strong>${t.usuario}</strong>`;
                contenedorTestimonios.appendChild(p);
            });
        }

    } catch (err) {
        mensajeCarga.style.display = 'block';
        mensajeCarga.className = 'mensaje-estado error';
        mensajeCarga.textContent = 'No se pudo conectar con el servidor backend (' + err.message + ').';
    }
}

document.addEventListener('DOMContentLoaded', cargarDatosPublicos);
