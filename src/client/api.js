// for $assert(..)
import "./reactive.js";

function apiRequest(endpoint, method, body) {
  $assert(typeof(endpoint) === "string" && endpoint.charAt(0) === '/', "must specify a valid endpoint (it has to start with /)");
  $assert(typeof(method) === "string");
  const headers = body == null ? undefined : { "Content-Type": "application/json" };
  const _body = body == null ? undefined : JSON.stringify(body);
  return fetch(`/api${endpoint}`, {
    method: method,
    headers: headers,
    body: _body
  });
}

export function get(endpoint) {
  return apiRequest(endpoint, "GET");
}

export function post(endpoint, body) {
  $assert(body != null, "must specify a body when doing a POST request");
  return apiRequest(endpoint, "POST", body);
}

