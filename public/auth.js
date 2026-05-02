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

  function getUsers() {
    seedUsers();
    try {
      const users = JSON.parse(localStorage.getItem(USERS_KEY) || "[]");
      return Array.isArray(users) ? users : [];
    } catch (e) {
      return [];
    }
  }

  function saveUsers(users) {
    localStorage.setItem(USERS_KEY, JSON.stringify(users));
  }

  function getSession() {
    try {
      const session = JSON.parse(localStorage.getItem(SESSION_KEY) || "null");
      if (!session || !session.email || !session.role) return null;
      return session;
    } catch (e) {
      return null;
    }
  }

  function saveSession(user) {
    const session = {
      email: user.email,
      role: user.role,
      loginEm: new Date().toISOString()
    };
    localStorage.setItem(SESSION_KEY, JSON.stringify(session));
  }

  function logout() {
    localStorage.removeItem(SESSION_KEY);
    window.location.href = "Login.HTML";
  }

  function login(email, senha, tag) {
    const users = getUsers();
    const emailNormalizado = String(email || "").trim().toLowerCase();
    const senhaDigitada = String(senha || "");
    const tagDigitada = normalizarTag(tag);

    const user = users.find((u) => String(u.email).toLowerCase() === emailNormalizado);
    if (!user) {
      return { ok: false, mensagem: "Usuário não encontrado." };
    }

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

    saveSession(user);
    return { ok: true, user };
  }

  function criarConta(dados) {
    const users = getUsers();
    const email = String(dados.email || "").trim().toLowerCase();
    const senha = String(dados.senha || "");
    const confirmarSenha = String(dados.confirmarSenha || "");
    const role = dados.role === "admin" ? "admin" : "comum";
    const tag = normalizarTag(dados.tag || "");

    if (!email || !senha) {
      return { ok: false, mensagem: "Preencha e-mail e senha." };
    }

    if (senha.length < 6) {
      return { ok: false, mensagem: "A senha deve ter pelo menos 6 caracteres." };
    }

    if (senha !== confirmarSenha) {
      return { ok: false, mensagem: "As senhas não conferem." };
    }

    const existe = users.some((u) => String(u.email).toLowerCase() === email);
    if (existe) {
      return { ok: false, mensagem: "Este e-mail já está cadastrado." };
    }

    if (role === "admin" && !tag) {
      return { ok: false, mensagem: "Conta admin exige tag UID." };
    }

    users.push({
      email,
      senha,
      role,
      tag: role === "admin" ? tag : ""
    });
    saveUsers(users);

    return { ok: true, mensagem: "Conta criada com sucesso." };
  }

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

  function protegerPagina(opcoes) {
    const config = opcoes || {};
    const allowAnonymous = !!config.allowAnonymous;
    const allowedRoles = Array.isArray(config.allowedRoles) ? config.allowedRoles : [];
    const session = getSession();

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
