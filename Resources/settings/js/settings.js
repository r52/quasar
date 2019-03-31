var websocket = null;
var initialized = false;

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function getTableSelections(table) {
    return $.map(table.bootstrapTable('getSelections'), function (row) {
        return row.command
    })
}

function initialize_global(data) {
    var dat = data["data"]["settings"]["global"];
    $("input#global\\/dataport").val(dat.dataport);
    $("input[name='global\\/loglevel']").val([dat.loglevel]);
    $("textarea#global\\/cookies").val(dat.cookies);
    $("input#global\\/savelog").checked = dat.savelog;
    $("input#global\\/startup").checked = dat.startup;
}

function createExtensionTab(ext) {
    // add the links
    var a = `<a class="dropdown-item" data-toggle="tab" href="#${ext.name}" aria-controls="${ext.name}" aria-selected="false">${ext.fullname}</a>`
    $('#extension-links').append(a);

    // generate rate settings
    var rates = "";
    ext.rates.forEach(function (r) {
        var rinput = "";
        if (r.rate > 0) {
            rinput = `<input type="number" class="form-control" id="${ext.name}/${r.name}/rate" placeholder="ms" min="1" max="2147483647" step="1" value="${r.rate}">`
        }
        var rgroup = `
        <div class="form-group">
            <label class="font-weight-bold" for="${ext.name}/${r.name}/rate">${r.name}</label>
            <div class="custom-control custom-switch">
                <input type="checkbox" class="custom-control-input" id="${ext.name}/${r.name}/enabled" ${r.enabled ? 'checked' : ''} ${r.rate < 0 ? 'disabled' : ''}>
                <label class="custom-control-label" for="${ext.name}/${r.name}/enabled">Enabled</label>
            </div>
            ${rinput}
        </div>
        `
        rates += rgroup;
    });

    // generate extension settings
    var extsettings = "";
    if (ext.settings != null) {
        extsettings += "<h2>Extension Settings</h2>";

        ext.settings.forEach(function (s) {
            var element = "";

            if (s.type === "int" || s.type === "double" || s.type === "bool" || s.type === "string") {
                var minmax = "";
                var type = "text";

                if (s.type === "int" || s.type === "double") {
                    type = "number";
                    minmax = `min="${s.min}" max="${s.max}" step="${s.step}"`;
                } else if (s.type === "bool") {
                    type = "checkbox";
                } else if (s.type === "string") {
                    type = "text";
                }

                element = `<input type="${type}" class="form-control" id="${ext.name}/${s.name}" ${minmax} value="${s.val}">`;
            } else if (s.type === "select") {
                var options = "";
                s.list.forEach(function (l) {
                    options += `<option value="${l.value}" ${s.val === l.value ? 'selected' : ''}>${l.name}</option>`;
                });
                element = `
                <select class="custom-select" id="${ext.name}/${s.name}">
                    ${options}
                </select>
                `;
            }

            var sinput = `
            <div class="form-group">
                <label class="font-weight-bold" for="${ext.name}/${s.name}">${s.desc}</label>
                <div class="input-group mb-3">
                    ${element}
                </div>
            </div>
            `
            extsettings += sinput;
        });
    }

    // generate tab html
    var t = `
    <div class="tab-pane fade container-fluid" id="${ext.name}" role="tabpanel" aria-labelledby="extensions-tab">
        <h1>${ext.fullname}</h1>
        <dl class="row">
            <dt class="col-sm-3">Extension Identifier</dt>
            <dd class="col-sm-9">${ext.name}</dd>
            <dt class="col-sm-3">Description</dt>
            <dd class="col-sm-9">${ext.description}</dd>
            <dt class="col-sm-3">Author</dt>
            <dd class="col-sm-9">${ext.author}</dd>
            <dt class="col-sm-3">Version</dt>
            <dd class="col-sm-9">${ext.version}</dd>
            <dt class="col-sm-3">Website</dt>
            <dd class="col-sm-9">${ext.url}</dd>
        </dl>
        <h2>Data Sources</h2>
        ${rates}
        ${extsettings}
    </div>
    `
    $('#tab_content').append(t);
}

function createExtensionPages(data) {
    var dat = data["data"]["settings"]["extensions"];
    if (dat.length > 0) {
        $('#extension-links').empty();
        dat.forEach(function (e) {
            createExtensionTab(e);
        });
    }
}

function createLauncherPage(data) {
    var $ltable = $('#launcher-table');
    var $lremove = $('#remove-launch-command');

    // generate bootstrap table
    $ltable.bootstrapTable({
        data: data["data"]["settings"]["launcher"],
        striped: true,
        clickToSelect: true,
        idField: 'command',
        columns: [{
                field: 'state',
                checkbox: true,
                align: 'center',
                valign: 'middle'
            }, {
                field: 'command',
                title: 'Command'
            },
            {
                field: 'file',
                title: 'File/Commandline'
            },
            {
                field: 'args',
                title: 'Arguments'
            },
            {
                field: 'start',
                title: "Start Path"
            },
            {
                field: 'icon',
                title: 'Icon',
                formatter: function (value) {
                    if (value) {
                        return '<img class="icon" src="' + value + '"/>';
                    }
                    return "";
                }
            }
        ]
    });

    // delete button function
    $ltable.on('check.bs.table uncheck.bs.table ' +
        'check-all.bs.table uncheck-all.bs.table',
        function () {
            $lremove.prop('disabled', !$ltable.bootstrapTable('getSelections').length)
        });

    $lremove.click(function (evt) {
        evt.preventDefault();
        var cmds = getTableSelections($ltable);
        $ltable.bootstrapTable('remove', {
            field: 'command',
            values: cmds
        })
        $lremove.prop('disabled', true);
    });

    // debug button
    $('#launch-test').click(function (evt) {
        evt.preventDefault();
        var dat = $ltable.bootstrapTable('getData')
        dat.forEach(function (e) {
            delete e.state;
        });
        console.log(dat);
        console.log(JSON.stringify(dat));
    });

    // launcher icon functions
    $("#launcher-icon").change(function () {
        if (this.files && this.files[0]) {
            var reader = new FileReader();

            reader.onload = function (e) {
                $('#icon-preview').attr('src', e.target.result);
            }

            reader.readAsDataURL(this.files[0]);
        }
    });

    // launcher add modal function
    $('#launcher-save').click(function () {
        var entry = {
            command: $('#command-name').val(),
            file: $('#file-name').val(),
            start: $('#start-path').val(),
            args: $('#arguments').val(),
            icon: $('#icon-preview').attr('src')
        };

        if (!(entry.command && entry.file)) {
            if (!entry.command) {
                $('#command-name').addClass("is-invalid");
            }
            if (!entry.file) {
                $('#file-name').addClass("is-invalid");
            }
            return;
        }

        var dat = $ltable.bootstrapTable('getData')

        if (dat.find(e => e.command === entry.command)) {
            $('#command-name').addClass("is-invalid");
            return;
        }

        $('#command-name').val("");
        $('#file-name').val("");
        $('#start-path').val("");
        $('#arguments').val("");
        $("#launcher-icon").val("");
        $('#icon-preview').attr('src', "");

        $ltable.bootstrapTable('append', entry);
        $('#addModal').modal('hide');
    });
}

function save_settings() {
    var dat = {};

    // global tab
    $("div[aria-labelledby='global-tab'] :input").each(function () {
        var input = $(this);
        var key = input.attr("name");

        if (key == undefined) {
            key = input.attr("id");
        }

        var type = input.attr("type");

        if (type == "checkbox") {
            dat[key] = input.prop("checked");
        } else if (type == "radio") {
            if (input.prop("checked") == true) {
                dat[key] = input.val();
            }
            // else do nothing
        } else if (type == "number") {
            dat[key] = parseInt(input.val());
        } else {
            dat[key] = input.val();
        }
    });

    // extension tabs
    dat["extensions"] = {};
    $("div[aria-labelledby='extensions-tab'] :input").each(function () {
        var input = $(this);
        var key = input.attr("name");

        if (key == undefined) {
            key = input.attr("id");
        }

        var parts = key.split("/");
        var extname = parts[0];

        if (!(extname in dat["extensions"])) {
            // create key if not exist
            dat["extensions"][extname] = {};
        }

        var type = input.attr("type");

        if (type == "checkbox") {
            dat["extensions"][extname][key] = input.prop("checked");
        } else if (type == "radio") {
            if (input.prop("checked") == true) {
                dat["extensions"][extname][key] = input.val();
            }
            // else do nothing
        } else if (type == "number") {
            dat["extensions"][extname][key] = parseInt(input.val());
        } else {
            dat["extensions"][extname][key] = input.val();
        }
    });

    // do launcher table
    var tab = $('#launcher-table').bootstrapTable('getData')
    tab.forEach(function (e) {
        delete e.state;
    });

    dat["launcher"] = tab;

    // send it
    var msg = {
        method: "mutate",
        params: {
            target: "settings",
            params: dat
        }
    };

    websocket.send(JSON.stringify(msg));

    var notify = $.notify({
        title: "Success!",
        message: "All settings have been saved."
    }, {
        type: "success",
        allow_dismiss: false,
        placement: {
            from: "bottom",
            align: "right"
        },
        offset: {
            x: 20,
            y: 20
        },
        animate: {
            enter: 'animated fadeInUp',
            exit: 'animated fadeOutDown'
        },
        spacing: 10,
        z_index: 1031,
        delay: 2000
    });
}

function query_settings(socket) {
    var msg = {
        method: "query",
        params: {
            target: "settings",
            params: "all"
        }
    };

    socket.send(JSON.stringify(msg));
}

async function do_on_connect(socket) {
    quasar_authenticate(socket);
    await sleep(10);
    query_settings(socket);
}

function initialize_page(dat) {
    if (!initialized) {
        initialize_global(dat);

        createExtensionPages(dat);
        createLauncherPage(dat);

        $("#cover").hide();

        initialized = true;
    }
}

function parse_data(msg) {
    var data = JSON.parse(msg);

    if ("data" in data && "settings" in data["data"]) {
        initialize_page(data);
    }
}

$(function () {
    try {
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = quasar_create_websocket();
        websocket.onopen = function (evt) {
            do_on_connect(websocket);
        };
        websocket.onmessage = function (evt) {
            parse_data(evt.data);
        };
        websocket.onerror = function (evt) {
            console.log('ERROR: ' + evt.data);
        };
    } catch (exception) {
        console.log('Exception: ' + exception);
    }

    // save button
    $('#settings-save').click(function () {
        save_settings();
    });

    // close button
    $('#settings-close').click(function () {
        close();
    });
});
