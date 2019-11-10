{
    const elems = document.getElementsByClassName('popUpLink');
    for (let i = 0; i < elems.length; ++i) {
        const elem = elems[i];
        if (elem.tagName.toLowerCase() == 'a') {
            elem.addEventListener('click', function (e) {
                let w = window.open();
                w.opener = null;
                w.location = elem.href;
                e.preventDefault();
            });
        }
    }
}
