const STORAGE_KEY = "UserID";

export function isSessionStorage() {
  return (new URLSearchParams(window.location.search)).has("session");
}

export function getOrCreateID(useSessionStorage) {
  if(typeof useSessionStorage !== "boolean") {
    useSessionStorage = isSessionStorage();
  }
  const storage = useSessionStorage ? sessionStorage : localStorage;
  if (!storage.getItem(STORAGE_KEY)) {
    const newID = +new Date() + "-" + Math.random();
    storage.setItem(STORAGE_KEY, newID);
  }
  return storage.getItem(STORAGE_KEY);
}

export function clearID(useSessionStorage) {
  if(typeof useSessionStorage !== "boolean") {
    useSessionStorage = isSessionStorage();
  }
  const storage = useSessionStorage ? sessionStorage : localStorage;
  storage.clear();
}
