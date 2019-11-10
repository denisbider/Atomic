const crumbDiv = Elem(arg.crumbDivId);
const crumbLink = Elem(arg.crumbLinkId);
const mainDiv = Elem(arg.mainDivId);
const mainLink = Elem(arg.mainLinkId);

crumbLink.addEventListener('click', function (e) {
    crumbDiv.style.display = 'none';
    mainDiv.style.display = 'block';
    e.preventDefault();
});

mainDiv.addEventListener('click', function (e) {
    crumbDiv.style.display = 'block';
    mainDiv.style.display = 'none';
    e.preventDefault();
});
