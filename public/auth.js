(function () {
  const USERS_KEY = "sistemaUsuarios";
  const SESSION_KEY = "sistemaSessao";

  const DEFAULT_USERS = [
    {
      email: "admin@rfid.com",
      senha: "admin123",
      role: "admin",
      tag: "33 9D F7 0D"
    },
    {
      email: "usuario@rfid.com",
      senha: "usuario123",
      role: "comum",
      tag: ""
    }
  ];

  function normalizarTag(tag) {
    return String(tag || "").trim().replace(/\s+/g, " ").toUpperCase();
  }

  function seedUsers() {
    const dados = localStorage.getItem(USERS_KEY);
    if (!dados) {
      localStorage.setItem(USERS_KEY, JSON.stringify(DEFAULT_USERS));
    }
  }
//pega os usuários
  function getUsers() {
    seedUsers();
    try {
      const users = JSON.parse(localStorage.getItem(USERS_KEY) || "[]");
      return Array.isArray(users) ? users : [];
    } catch (e) {
      return [];
    }
  }
//Salva a lista atualizada
  function saveUsers(users) {
    localStorage.setItem(USERS_KEY, JSON.stringify(users));
  }
// Vê se tem alguém logado
  function getSession() {
    try {
      const session = JSON.parse(localStorage.getItem(SESSION_KEY) || "null");
      if (!session || !session.email || !session.role) return null;
      return session;
    } catch (e) {
      return null;
    }
  }
// Salva a sessão atualizada
  function saveSession(user) {
    const session = {
      email: user.email,
      role: user.role,
      loginEm: new Date().toISOString()
    };
    localStorage.setItem(SESSION_KEY, JSON.stringify(session));
  }
// Desloga o usuário
  function logout() {
    localStorage.removeItem(SESSION_KEY);
    window.location.href = "Login.HTML";
  }
// Faz o login do usuário
  function login(email, senha, tag) {
    const users = getUsers();
    const emailNormalizado = String(email || "").trim().toLowerCase();
    const senhaDigitada = String(senha || "");
    const tagDigitada = normalizarTag(tag);
// Encontra o usuário
    const user = users.find((u) => String(u.email).toLowerCase() === emailNormalizado);
    if (!user) {
      return { ok: false, mensagem: "Usuário não encontrado." };
    }
// Verifica se a senha está correta
    if (String(user.senha) !== senhaDigitada) {
      return { ok: false, mensagem: "Senha inválida." };
    }

    if (user.role === "admin") {
      const tagCadastrada = normalizarTag(user.tag);
      if (!tagDigitada) {
        return { ok: false, mensagem: "Para admin, a tag é obrigatória." };
      }
      if (tagDigitada !== tagCadastrada) {
        return { ok: false, mensagem: "Tag de administrador inválida." };
      }
    }
// Salva a sessão atualizada
    saveSession(user);
    return { ok: true, user };
  }
// Cria uma nova conta
  function criarConta(dados) {
    const users = getUsers();
    const email = String(dados.email || "").trim().toLowerCase();
    const senha = String(dados.senha || "");
    const confirmarSenha = String(dados.confirmarSenha || "");
    const role = dados.role === "admin" ? "admin" : "comum";
    const tag = normalizarTag(dados.tag || "");
// Verifica se o e-mail e a senha estão preenchidos
    if (!email || !senha) {
      return { ok: false, mensagem: "Preencha e-mail e senha." };
    }
// Verifica se a senha tem pelo menos 6 caracteres
    if (senha.length < 6) {
      return { ok: false, mensagem: "A senha deve ter pelo menos 6 caracteres." };
    }
// Verifica se as senhas conferem
    if (senha !== confirmarSenha) {
      return { ok: false, mensagem: "As senhas não conferem." };
    }
// Verifica se o e-mail já está cadastrado
    const existe = users.some((u) => String(u.email).toLowerCase() === email);
    if (existe) {
      return { ok: false, mensagem: "Este e-mail já está cadastrado." };
    }
// Verifica se a conta admin precisa de uma tag
    if (role === "admin" && !tag) {
      return { ok: false, mensagem: "Conta admin exige tag UID." };
    }
// Adiciona o novo usuário à lista      
    users.push({
      email,
      senha,
      role,
      tag: role === "admin" ? tag : ""
    });
    saveUsers(users);
// Retorna uma mensagem de sucesso
    return { ok: true, mensagem: "Conta criada com sucesso." };
  }
// Aplica o menu por role
  function aplicarMenuPorRole(role) {
    const links = document.querySelectorAll(".menu a");
    links.forEach((link) => {
      const href = link.getAttribute("href") || "";
      if (role === "comum" && href.toLowerCase() === "salas-vinculadas.html") {
        link.style.display = "none";
      }
      if (!role && href.toLowerCase() !== "login.html" && href.toLowerCase() !== "sobre.html") {
        link.style.display = "none";
      }
    });
  }
// Aplica as restrições visuais
  function aplicarRestricoesVisuais(role) {
    const blocos = document.querySelectorAll("[data-role-required]");
    blocos.forEach((bloco) => {
      const roles = String(bloco.getAttribute("data-role-required") || "")
        .split(",")
        .map((r) => r.trim())
        .filter(Boolean);
      if (!roles.length) return;
      if (!roles.includes(role)) {
        bloco.style.display = "none";
      }
    });
  }
// Adiciona o botão de logout     
  function adicionarBotaoLogout(session) {
    if (!session) return;
    const menu = document.querySelector(".menu");
    if (!menu) return;

    const jaExiste = document.getElementById("btn-logout-menu");
    if (jaExiste) return;

    const botao = document.createElement("a");
    botao.id = "btn-logout-menu";
    botao.className = "menu-logout";
    botao.href = "#";
    botao.textContent = "Deslogar";
    botao.addEventListener("click", function (event) {
      event.preventDefault();
      logout();
    });
    menu.appendChild(botao);
  }
// Protege a página           
  function protegerPagina(opcoes) {
    const config = opcoes || {};
    const allowAnonymous = !!config.allowAnonymous;
    const allowedRoles = Array.isArray(config.allowedRoles) ? config.allowedRoles : [];
    const session = getSession();
// Verifica se o usuário está logado
    if (!session && !allowAnonymous) {
      window.location.href = "Login.HTML";
      return null;
    }

    if (session && allowedRoles.length && !allowedRoles.includes(session.role)) {
      if (session.role === "comum") {
        window.location.href = "historico.html";
      } else {
        window.location.href = "Patrimônios.html";
      }
      return null;
    }

    aplicarMenuPorRole(session ? session.role : "");
    aplicarRestricoesVisuais(session ? session.role : "");
    adicionarBotaoLogout(session);
    return session;
  }

  window.AuthApp = {
    getSession,
    login,
    logout,
    criarConta,
    protegerPagina
  };
})();
