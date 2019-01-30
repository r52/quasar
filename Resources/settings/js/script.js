var sample_data = {
    "data": {
        "settings": {
            "general": {
                "dataport": 13337,
                "loglevel": 2,
                "cookies": "",
                "savelog": true,
                "startup": true
            },
            "extensions": [{
                    "name": "simple_perf",
                    "fullname": "Simple Performance Query",
                    "version": "v1",
                    "author": "me",
                    "description": "Sample extension providing basic performance metrics",
                    "website": "https://github.com/r52/quasar",
                    "rates": [{
                            "name": "cpu",
                            "enabled": true,
                            "rate": 5000
                        },
                        {
                            "name": "ram",
                            "enabled": true,
                            "rate": 5000
                        }
                    ],
                    "settings": null
                },
                {
                    "name": "win_audio_viz",
                    "fullname": "Audio Visualization",
                    "version": "v1",
                    "author": "me",
                    "description": "Provides desktop audio frequency data",
                    "website": "https://github.com/r52/quasar",
                    "rates": [{
                        "name": "viz",
                        "enabled": true,
                        "rate": null
                    }],
                    "settings": [{
                            "name": "fftsize",
                            "desc": "FFT Size",
                            "type": "int",
                            "min": 0,
                            "max": 8192,
                            "step": 2,
                            "def": 256,
                            "val": 256
                        },
                        {
                            "name": "sensitivity",
                            "desc": "Sensitivity",
                            "type": "double",
                            "min": 0.0,
                            "max": 10000.0,
                            "step": 0.1,
                            "def": 50.0,
                            "val": 50.0
                        }
                    ]
                }
            ],
            "launcher": [{
                    "command": "chrome",
                    "file": "cmd",
                    "args": "/c start chrome",
                    "start": "",
                    "icon": ""
                },
                {
                    "command": "explorer",
                    "file": "C:/Windows/explorer.exe",
                    "args": "",
                    "start": "",
                    "icon": ""
                }
            ]
        }
    }
};

function getTableSelections(table) {
    return $.map(table.bootstrapTable('getSelections'), function(row) {
        return row.command
    })
}

function createExtensionTab(ext) {
    // add the links
    var a = `<a class="dropdown-item" data-toggle="tab" href="#${ext.name}">${ext.fullname}</a>`
    $('#extension-links').append(a);

    // generate rate settings
    var rates = "";
    ext.rates.forEach(function(r){
        var rinput = "";
        if (r.rate != null) {
            rinput = `<input type="number" class="form-control" id="${ext.name}/${r.name}-rate" placeholder="ms" min="1" max="2147483647" step="1" value="${r.rate}">`
        }
        var rgroup = `
        <div class="form-group">
            <label for="${ext.name}/${r.name}-rate">${r.name}</label>
            <div class="custom-control custom-switch">
                <input type="checkbox" class="custom-control-input" id="${ext.name}/${r.name}-enabled" ${r.enabled ? 'checked' : ''}>
                <label class="custom-control-label" for="${ext.name}/${r.name}-enabled">Enabled</label>
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

        ext.settings.forEach(function(s) {
            var sinput = `
            <div class="form-group">
                <label for="${ext.name}/${s.name}">${s.desc}</label>
                <div class="input-group mb-3">
                    <input type="${(s.type === "int" || s.type === "double") ? 'number' : 'checkbox'}" class="form-control" id="${ext.name}/${s.name}" min="${s.min}" max="${s.max}" step="${s.step}" value="${s.val}">
                </div>
            </div>
            `
            extsettings += sinput;
        });
    }

    // generate tab html
    var t = `
    <div class="tab-pane fade" id="${ext.name}" role="tabpanel" aria-labelledby="extensions-tab">
        <h1>${ext.fullname}</h1>
        <dl class="row">
            <dt class="col-sm-3">Extension Code</dt>
            <dd class="col-sm-9">${ext.name}</dd>
            <dt class="col-sm-3">Description</dt>
            <dd class="col-sm-9">${ext.description}</dd>
            <dt class="col-sm-3">Author</dt>
            <dd class="col-sm-9">${ext.author}</dd>
            <dt class="col-sm-3">Version</dt>
            <dd class="col-sm-9">${ext.version}</dd>
            <dt class="col-sm-3">Website</dt>
            <dd class="col-sm-9">${ext.website}</dd>
        </dl>
        <h2>Data Sources</h2>
        ${rates}
        ${extsettings}
    </div>
    `
    $('#tab_content').append(t);
}

function createExtensionPages() {
    var dat = sample_data["data"]["settings"]["extensions"];
    dat.forEach(function(e) {
        createExtensionTab(e);
    });
}

function createLauncherPage() {
    var $ltable = $('#launcher-table');
    var $lremove = $('#remove-launch-command');

    // generate bootstrap table
    $ltable.bootstrapTable({
        data: sample_data["data"]["settings"]["launcher"],
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
                formatter: function(value) {
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
        function() {
            $lremove.prop('disabled', !$ltable.bootstrapTable('getSelections').length)
        });

    $lremove.click(function(evt) {
        evt.preventDefault();
        var cmds = getTableSelections($ltable);
        $ltable.bootstrapTable('remove', {
            field: 'command',
            values: cmds
        })
        $lremove.prop('disabled', true);
    });

    // debug button
    $('#launch-test').click(function(evt) {
        evt.preventDefault();
        var dat = $ltable.bootstrapTable('getData')
        dat.forEach(function(e) {
            delete e.state;
        });
        console.log(dat);
        console.log(JSON.stringify(dat));
    });

    // launcher icon functions
    $("#launcher-icon").change(function() {
        if (this.files && this.files[0]) {
            var reader = new FileReader();

            reader.onload = function(e) {
                $('#icon-preview').attr('src', e.target.result);
            }

            reader.readAsDataURL(this.files[0]);
        }
    });

    // launcher add modal function
    $('#launcher-save').click(function() {
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

$(function() {
    createExtensionPages();
    createLauncherPage();

    // close button
    $('#settings-close').click(function() {
        close();
    });

    // TODO: Save button
});
