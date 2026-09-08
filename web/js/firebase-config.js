import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import {
  getDatabase,
  get,
  onValue,
  ref,
  remove,
  serverTimestamp,
  set,
  update
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";

export const FIREBASE_CONFIG = {
  apiKey: "AIzaSyC59Yydxl5l5vK6DBOnyzpy49jc3jES3lg",
  authDomain: "voltv1.firebaseapp.com",
  databaseURL: "https://voltv1-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "voltv1",
  storageBucket: "voltv1.firebasestorage.app",
  messagingSenderId: "230780428660",
  appId: "1:230780428660:web:d5707289b9600847b4cc21"
};

export const DEVICE_ROOT = "voltix";
export const DEVICE_ID = "voltix-001";

const app = initializeApp(FIREBASE_CONFIG);
export const db = getDatabase(app);
export { get, onValue, ref, remove, serverTimestamp, set, update };
