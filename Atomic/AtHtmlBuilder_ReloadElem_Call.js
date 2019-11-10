const outElemId    = arg.outElemId;
const statusElemId = arg.statusElemId;
const url          = arg.url;
const method       = arg.method      || 'GET';
const firstWaitMs  = arg.firstWaitMs || 5000;
const retryMs      = arg.retryMs     || 3000;

Elem(statusElemId).innerHTML = '[ Loading... ]';
ReloadElem(outElemId, statusElemId, method, url, firstWaitMs, retryMs);
