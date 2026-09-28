// auth.js -- Login y Registro (login.html)

const cajaLogin = document.getElementById('cajaLogin');
const cajaRegistro = document.getElementById('cajaRegistro');
const mensajeGlobal = document.getElementById('mensajeGlobal');

function mostrarMensaje(texto, tipo) {
    mensajeGlobal.style.display = 'block';
    mensajeGlobal.className = 'mensaje-estado ' + tipo;
    mensajeGlobal.textContent = texto;
}

// Si la URL trae ?registro=1, mostrar directamente el formulario de registro
if (new URLSearchParams(window.location.search).get('registro') === '1') {
    cajaLogin.style.display = 'none';
    cajaRegistro.style.display = 'block';
}

document.getElementById('irARegistro').addEventListener('click', (e) => {
    e.preventDefault();
    cajaLogin.style.display = 'none';
    cajaRegistro.style.display = 'block';
});

document.getElementById('irALogin').addEventListener('click', (e) => {
    e.preventDefault();
    cajaRegistro.style.display = 'none';
    cajaLogin.style.display = 'block';
});

function redirigirSegunRol(rol) {
    if (rol === 'CLIENTE') window.location.href = 'cliente.html';
    else if (rol === 'VENDEDOR') window.location.href = 'vendedor.html';
    else if (rol === 'GERENTE') window.location.href = 'gerente.html';
}

document.getElementById('formLogin').addEventListener('submit', async (e) => {
    e.preventDefault();
    const usuario = document.getElementById('loginUsuario').value.trim();
    const password = document.getElementById('loginPassword').value;

    try {
        const resp = await fetch('/api/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ usuario, password })
        });
        const data = await resp.json();

        if (data.status === 'success') {
            localStorage.setItem('skylinea_token', data.token);
            localStorage.setItem('skylinea_usuario', data.usuario);
            localStorage.setItem('skylinea_rol', data.rol);
            redirigirSegunRol(data.rol);
        } else {
            mostrarMensaje(data.mensaje || 'Credenciales invalidas', 'error');
        }
    } catch (err) {
        mostrarMensaje('No se pudo conectar con el servidor backend.', 'error');
    }
});

document.getElementById('formRegistro').addEventListener('submit', async (e) => {
    e.preventDefault();
    const usuario = document.getElementById('regUsuario').value.trim();
    const password = document.getElementById('regPassword').value;

    try {
        const resp = await fetch('/api/register', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ usuario, password })
        });
        const data = await resp.json();

        if (data.status === 'success') {
            mostrarMensaje('Cuenta creada correctamente. Ahora puedes iniciar sesion.', 'exito');
            cajaRegistro.style.display = 'none';
            cajaLogin.style.display = 'block';
        } else {
            mostrarMensaje(data.mensaje || 'No se pudo registrar', 'error');
        }
    } catch (err) {
        mostrarMensaje('No se pudo conectar con el servidor backend.', 'error');
    }
});
