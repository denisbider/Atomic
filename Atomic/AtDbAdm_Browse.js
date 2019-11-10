{
    const editLink = Elem('editLink');
    const entityJson = Elem('entityJson');
    const entitySave = Elem('entitySave');

    editLink.addEventListener('click', function (e) {
        if (entityJson.disabled) {
            editLink.innerHTML = 'Cancel';
            entityJson.disabled = false;
            entitySave.disabled = false;
        } else {
            editLink.innerHTML = 'Edit';
            entityJson.disabled = true;
            entitySave.disabled = true;
        }
        e.preventDefault();
    });
}
